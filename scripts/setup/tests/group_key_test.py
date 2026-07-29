import contextlib
import io
import os
import re
import stat
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


SETUP_DIR = Path(__file__).resolve().parents[1]
PROJECT_ROOT = SETUP_DIR.parents[1]
sys.path.insert(0, str(SETUP_DIR))

from core.error_handler import ConfigurationError
from core.validator import ConfigValidator
from managers.config_manager import ConfigManager
from managers.git_manager import ProjectStructureValidator
from workflows.configuration import ConfigurationWorkflow


VALID_KEY = 'a1' * 32
TEMPLATE_PLACEHOLDER = 'PROVISION_WITH_SETUP'


def string_defines_in_template():
    """Every define the template declares as a quoted C string."""
    template = (PROJECT_ROOT / 'include' / 'GlowConfig.h-template').read_text()
    return sorted(set(re.findall(r'^#define\s+(\w+)\s+"[^"]*"\s*$', template,
                                 re.MULTILINE)))


def make_manager(validator=None):
    """A ConfigManager without touching the real project directories."""
    manager = ConfigManager.__new__(ConfigManager)
    manager.validator = validator if validator is not None else ConfigValidator()
    return manager


class GroupKeyValidationTests(unittest.TestCase):
    def setUp(self):
        self.validator = ConfigValidator()

    def test_validator_requires_mesh_key_and_node_limit(self):
        base = {'MESH_ON': True, 'ESPNOW_CHANNEL': 1, 'GLOW_MAX_GROUP_NODES': 8}
        self.assertFalse(self.validator.validate_communication_config(base))
        self.assertFalse(self.validator.validate_communication_config({
            **base, 'GLOW_GROUP_KEY_HEX': '0' * 64,
        }))
        self.assertTrue(self.validator.validate_communication_config({
            **base, 'GLOW_GROUP_KEY_HEX': 'A1' * 32,
        }))

    def test_validator_rejects_a_wrong_group_size(self):
        base = {
            'MESH_ON': True,
            'ESPNOW_CHANNEL': 1,
            'GLOW_GROUP_KEY_HEX': VALID_KEY,
        }
        for size in (0, 1, 4, 9, 16):
            with self.subTest(size=size):
                self.assertFalse(self.validator.validate_communication_config({
                    **base, 'GLOW_MAX_GROUP_NODES': size,
                }))

    def test_normalize_rejects_everything_that_is_not_a_256_bit_hex_key(self):
        rejected = [
            None,
            123,
            b'a' * 64,
            '',
            TEMPLATE_PLACEHOLDER,
            'a' * 63,
            'a' * 65,
            'g' * 64,
            'a' * 63 + ' ',
            ' ' + 'a' * 63,
            'a' * 62 + '\n\n',
            '0x' + 'a' * 62,
            'a1:' * 21 + 'a',
            '0' * 64,
        ]
        for value in rejected:
            with self.subTest(value=repr(value)[:40]):
                self.assertIsNone(self.validator.normalize_group_key(value))

    def test_normalize_lowercases_a_valid_key(self):
        self.assertEqual(self.validator.normalize_group_key('AB' * 32), 'ab' * 32)
        self.assertEqual(self.validator.normalize_group_key('ab' * 32), 'ab' * 32)

    def test_format_group_key_never_reveals_the_secret(self):
        formatted = self.validator.format_group_key(VALID_KEY)
        self.assertNotIn(VALID_KEY, formatted)
        self.assertIn(VALID_KEY[-8:], formatted)
        # The fingerprint makes two lamps comparable without exposing the key.
        self.assertIn('SHA-256', formatted)

    def test_format_group_key_reports_unprovisioned_values(self):
        for value in (None, '', TEMPLATE_PLACEHOLDER, '0' * 64):
            with self.subTest(value=value):
                self.assertEqual(
                    self.validator.format_group_key(value), '<not provisioned>'
                )

    def test_two_keys_get_different_fingerprints(self):
        first = self.validator.format_group_key('a1' * 32)
        second = self.validator.format_group_key('a2' * 32)
        self.assertNotEqual(first, second)


class GroupKeyProvisioningTests(unittest.TestCase):
    def setUp(self):
        self.validator = ConfigValidator()

    def test_join_normalizes_key_and_does_not_print_it(self):
        manager = make_manager(self.validator)
        key = 'AB' * 32
        output = io.StringIO()
        with patch('builtins.input', side_effect=['y', '6', 'j']), \
                patch('getpass.getpass', return_value=key), \
                contextlib.redirect_stdout(output):
            config = manager.configure_esp_now()

        self.assertEqual(config['GLOW_GROUP_KEY_HEX'], key.lower())
        self.assertEqual(config['GLOW_MAX_GROUP_NODES'], 8)
        self.assertTrue(config['GLOW_SYNC_FOLLOW_DEFAULT'])
        self.assertTrue(config['GLOW_SYNC_PUBLISH_DEFAULT'])
        self.assertNotIn(key.lower(), output.getvalue().lower())
        self.assertIn(key[-8:].lower(), output.getvalue().lower())

    def test_join_reprompts_until_the_key_is_valid(self):
        manager = make_manager(self.validator)
        output = io.StringIO()
        with patch('builtins.input', side_effect=['y', '6', 'j', 'j', 'j']), \
                patch('getpass.getpass', side_effect=['too-short', '0' * 64, VALID_KEY]), \
                contextlib.redirect_stdout(output):
            config = manager.configure_esp_now()

        self.assertEqual(config['GLOW_GROUP_KEY_HEX'], VALID_KEY)

    def test_create_generates_a_fresh_random_key(self):
        manager = make_manager(self.validator)
        keys = set()
        for _ in range(3):
            output = io.StringIO()
            with patch('builtins.input', side_effect=['y', '6', 'c']), \
                    contextlib.redirect_stdout(output):
                config = manager.configure_esp_now()
            key = config['GLOW_GROUP_KEY_HEX']
            self.assertIsNotNone(self.validator.normalize_group_key(key))
            self.assertEqual(key, key.lower())
            self.assertNotIn(key, output.getvalue())
            keys.add(key)

        self.assertEqual(len(keys), 3, "generated keys must not repeat")

    def test_disabled_mesh_provisions_no_key(self):
        manager = make_manager(self.validator)
        output = io.StringIO()
        with patch('builtins.input', side_effect=['n']), \
                contextlib.redirect_stdout(output):
            config = manager.configure_esp_now()

        self.assertNotIn('GLOW_GROUP_KEY_HEX', config)
        self.assertFalse(config['GLOW_SYNC_FOLLOW_DEFAULT'])
        self.assertFalse(config['GLOW_SYNC_PUBLISH_DEFAULT'])


class PortalProvisioningTests(unittest.TestCase):
    def setUp(self):
        self.validator = ConfigValidator()

    def test_disabled_portal_needs_no_password(self):
        manager = make_manager(self.validator)
        with patch('builtins.input', side_effect=['n']):
            config = manager.configure_portal()
        self.assertEqual(config, {'GLOW_PORTAL_ENABLED': False})

    def test_enabled_portal_gets_a_unique_wpa2_password(self):
        manager = make_manager(self.validator)
        output = io.StringIO()
        with patch('builtins.input', side_effect=['y']), \
                patch('secrets.token_urlsafe', return_value='unique-portal-pass'), \
                contextlib.redirect_stdout(output):
            config = manager.configure_portal()
        self.assertTrue(config['GLOW_PORTAL_ENABLED'])
        self.assertEqual(config['GLOW_PORTAL_PASSWORD'], 'unique-portal-pass')
        self.assertTrue(self.validator.validate_portal_config(config))


class OtaProvisioningTests(unittest.TestCase):
    def setUp(self):
        self.validator = ConfigValidator()

    def test_disabled_ota_needs_no_password(self):
        manager = make_manager(self.validator)
        with patch('builtins.input', side_effect=['n']):
            config = manager.configure_ota()
        self.assertEqual(config, {'GLOW_OTA_ENABLED': False})

    def test_enabled_ota_gets_a_separate_password(self):
        manager = make_manager(self.validator)
        with patch('builtins.input', side_effect=['y']), \
                patch('secrets.token_urlsafe', return_value='unique-ota-password'):
            config = manager.configure_ota()
        self.assertTrue(config['GLOW_OTA_ENABLED'])
        self.assertEqual(config['GLOW_OTA_PASSWORD'], 'unique-ota-password')
        self.assertTrue(self.validator.validate_ota_config(config))


class ConfigWritingTests(unittest.TestCase):
    def setUp(self):
        self.validator = ConfigValidator()

    def test_generated_define_is_quoted_and_redacted(self):
        manager = make_manager(self.validator)
        key = '12' * 32
        content = f'#define GLOW_GROUP_KEY_HEX "{TEMPLATE_PLACEHOLDER}"\n'
        generated = manager._update_define(content, 'GLOW_GROUP_KEY_HEX', key)

        self.assertEqual(generated, f'#define GLOW_GROUP_KEY_HEX "{key}"\n')
        redacted = manager.redact_config_content(generated)
        self.assertNotIn(key, redacted)
        self.assertIn(key[-8:], redacted)

    def test_portal_password_is_quoted_and_redacted(self):
        manager = make_manager(self.validator)
        content = '#define GLOW_PORTAL_PASSWORD "PROVISION_WITH_SETUP"\n'
        generated = manager._update_define(
            content, 'GLOW_PORTAL_PASSWORD', 'unique-portal-pass')
        self.assertIn('"unique-portal-pass"', generated)
        self.assertNotIn('unique-portal-pass', manager.redact_config_content(generated))

    def test_ota_password_is_quoted_and_redacted(self):
        manager = make_manager(self.validator)
        content = '#define GLOW_OTA_PASSWORD "PROVISION_WITH_SETUP"\n'
        generated = manager._update_define(
            content, 'GLOW_OTA_PASSWORD', 'unique-ota-password')
        self.assertIn('"unique-ota-password"', generated)
        self.assertNotIn('unique-ota-password', manager.redact_config_content(generated))

    def test_new_fields_are_added_to_an_existing_config(self):
        manager = make_manager(self.validator)
        key = '34' * 32
        content = '#define MESH_ON true\n'

        content = manager._update_define(content, 'GLOW_GROUP_KEY_HEX', key)
        content = manager._update_define(content, 'GLOW_MAX_GROUP_NODES', 8)

        self.assertIn(f'#define GLOW_GROUP_KEY_HEX "{key}"', content)
        self.assertIn('#define GLOW_MAX_GROUP_NODES 8', content)

    def test_an_invalid_key_is_never_written(self):
        manager = make_manager(self.validator)
        content = f'#define GLOW_GROUP_KEY_HEX "{TEMPLATE_PLACEHOLDER}"\n'
        for value in ('0' * 64, 'a' * 63, TEMPLATE_PLACEHOLDER, 'zz' * 32, None):
            with self.subTest(value=repr(value)[:20]):
                with self.assertRaises(ConfigurationError):
                    manager._update_define(content, 'GLOW_GROUP_KEY_HEX', value)

    def test_redaction_covers_every_way_a_key_can_appear(self):
        manager = make_manager(self.validator)
        key = '56' * 32
        variants = [
            f'#define GLOW_GROUP_KEY_HEX "{key}"',
            f'#define  GLOW_GROUP_KEY_HEX  "{key}"',
            f'#define GLOW_GROUP_KEY_HEX "{key}"  // shared group secret',
            f'#define GLOW_GROUP_KEY_HEX {key}',
        ]
        for line in variants:
            with self.subTest(line=line[:45]):
                redacted = manager.redact_config_content(line + '\n')
                self.assertNotIn(key, redacted)

    def test_every_password_define_is_redacted(self):
        manager = make_manager(self.validator)
        secrets_by_define = {
            'WIFI_PASSWORD': 'my-wifi-secret',
            'GLOW_PORTAL_PASSWORD': 'my-portal-secret',
            'GLOW_OTA_PASSWORD': 'my-ota-secret',
        }
        content = ''.join(
            f'#define {define} "{value}"\n'
            for define, value in secrets_by_define.items()
        )
        redacted = manager.redact_config_content(content)

        for define, value in secrets_by_define.items():
            with self.subTest(define=define):
                self.assertNotIn(value, redacted)
                self.assertIn(f'#define {define} "<redacted>"', redacted)

    def test_every_string_define_is_written_quoted(self):
        # An unquoted string ends up in the header as bare C tokens and breaks
        # the build, which is exactly how the MQTT broker host got through.
        manager = make_manager(self.validator)
        samples = {
            'GLOW_GROUP_KEY_HEX': VALID_KEY,
            'GLOW_PORTAL_PASSWORD': 'portal-password',
            'GLOW_OTA_PASSWORD': 'ota-password-long',
        }
        for define in string_defines_in_template():
            value = samples.get(define, 'broker.example')
            with self.subTest(define=define):
                line = f'#define {define} "PLACEHOLDER"\n'
                written = manager._update_define(line, define, value)
                self.assertRegex(
                    written,
                    rf'^#define {define} "[^"]*"$',
                    f'{define} was not written as a quoted C string: {written!r}',
                )

    def test_a_missing_define_is_appended_for_every_known_setting(self):
        manager = make_manager(self.validator)
        samples = {
            'GLOW_GROUP_KEY_HEX': VALID_KEY,
            'GLOW_PORTAL_PASSWORD': 'portal-password',
            'GLOW_OTA_PASSWORD': 'ota-password-long',
        }
        for define in string_defines_in_template():
            value = samples.get(define, 'example')
            with self.subTest(define=define):
                written = manager._update_define('#define MESH_ON true\n', define, value)
                self.assertIn(f'#define {define} "', written)

    def test_known_defines_all_exist_in_the_template(self):
        template = (PROJECT_ROOT / 'include' / 'GlowConfig.h-template').read_text()
        declared = set(re.findall(r'^#define\s+(\w+)', template, re.MULTILINE))
        self.assertEqual(set(ConfigManager.KNOWN_DEFINES) - declared, set())

    def test_a_string_value_with_a_quote_is_refused(self):
        manager = make_manager(self.validator)
        for bad in ('has"quote', 'has\nnewline', 'trailing\\'):
            with self.subTest(value=repr(bad)):
                with self.assertRaises(ConfigurationError):
                    manager._update_define(
                        '#define GLOW_MQTT_HOST ""\n', 'GLOW_MQTT_HOST', bad)

    def test_the_template_declares_no_secret_the_redaction_misses(self):
        # A new password define that nobody adds to SECRET_DEFINES would be
        # printed in the clear by the config view and every backup listing.
        template = (PROJECT_ROOT / 'include' / 'GlowConfig.h-template').read_text()
        declared = set(re.findall(r'^#define\s+(\w*PASSWORD\w*)\s', template,
                                  re.MULTILINE))
        self.assertTrue(declared, 'expected the template to declare passwords')
        self.assertEqual(declared - set(ConfigManager.SECRET_DEFINES), set())

    def test_wifi_settings_are_validated(self):
        base = {
            'WIFI_ON': True,
            'WIFI_SSID': 'home',
            'WIFI_PASSWORD': 'password123',
            'GLOW_HOSTNAME': 'glow-bedroom',
        }
        self.assertTrue(self.validator.validate_wifi_config(base))
        # Disabled still needs a usable hostname, nothing else.
        self.assertTrue(self.validator.validate_wifi_config(
            {'WIFI_ON': False, 'GLOW_HOSTNAME': 'glowlight'}))
        # An open network is allowed.
        self.assertTrue(self.validator.validate_wifi_config({**base, 'WIFI_PASSWORD': ''}))

        rejected = [
            {**base, 'WIFI_SSID': ''},
            {**base, 'WIFI_SSID': 'x' * 33},
            {**base, 'WIFI_PASSWORD': 'short'},
            {**base, 'WIFI_PASSWORD': 'x' * 64},
            {**base, 'GLOW_HOSTNAME': ''},
            {**base, 'GLOW_HOSTNAME': '-leading'},
            {**base, 'GLOW_HOSTNAME': 'trailing-'},
            {**base, 'GLOW_HOSTNAME': 'has space'},
            {**base, 'WIFI_ON': 'yes'},
        ]
        for config in rejected:
            with self.subTest(config=str(config)[:60]):
                self.assertFalse(self.validator.validate_wifi_config(config))

    # The firmware rejects the whole configuration when Home Assistant is on
    # without WiFi, which also silences ESP-NOW. The setup must not produce it.
    def test_home_assistant_cannot_be_enabled_without_wifi(self):
        manager = make_manager(self.validator)
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            config = manager.configure_mqtt(wifi_enabled=False)

        self.assertEqual(config, {'GLOW_MQTT_ENABLED': False})
        self.assertIn('wifi', output.getvalue().lower())

    def test_wifi_password_is_not_echoed(self):
        manager = make_manager(self.validator)
        output = io.StringIO()
        secret = 'wifi-secret-1'
        with patch('builtins.input', side_effect=['y', 'home', 'glow-test']), \
                patch('getpass.getpass', return_value=secret), \
                contextlib.redirect_stdout(output):
            config = manager.configure_wifi()

        self.assertTrue(config['WIFI_ON'])
        self.assertEqual(config['WIFI_SSID'], 'home')
        self.assertEqual(config['WIFI_PASSWORD'], secret)
        self.assertEqual(config['GLOW_HOSTNAME'], 'glow-test')
        self.assertNotIn(secret, output.getvalue())

    def test_home_assistant_settings_are_validated(self):
        base = {
            'GLOW_MQTT_ENABLED': True,
            'GLOW_MQTT_HOST': 'mqtt.local',
            'GLOW_MQTT_PORT': 1883,
            'GLOW_MQTT_USER': 'glow',
            'GLOW_MQTT_PASSWORD': 'broker-secret',
            'GLOW_MQTT_DISCOVERY_PREFIX': 'homeassistant',
        }
        self.assertTrue(self.validator.validate_mqtt_config(base))
        # Disabled needs nothing else.
        self.assertTrue(self.validator.validate_mqtt_config({'GLOW_MQTT_ENABLED': False}))

        rejected = [
            {**base, 'GLOW_MQTT_HOST': ''},
            {**base, 'GLOW_MQTT_HOST': 'has space'},
            {**base, 'GLOW_MQTT_PORT': 0},
            {**base, 'GLOW_MQTT_PORT': 70000},
            {**base, 'GLOW_MQTT_PORT': True},
            {**base, 'GLOW_MQTT_DISCOVERY_PREFIX': ''},
            {**base, 'GLOW_MQTT_PASSWORD': 'x' * 65},
            {**base, 'GLOW_MQTT_ENABLED': 'yes'},
        ]
        for config in rejected:
            with self.subTest(config=str(config)[:60]):
                self.assertFalse(self.validator.validate_mqtt_config(config))

    def test_redaction_leaves_the_rest_of_the_config_intact(self):
        manager = make_manager(self.validator)
        content = (
            '#define LED_DATA_PIN 3\n'
            f'#define GLOW_GROUP_KEY_HEX "{VALID_KEY}"\n'
            '#define MESH_ON true\n'
        )
        redacted = manager.redact_config_content(content)

        self.assertIn('#define LED_DATA_PIN 3', redacted)
        self.assertIn('#define MESH_ON true', redacted)
        self.assertNotIn(VALID_KEY, redacted)


class DisplayRedactionTests(unittest.TestCase):
    """The key must not leak through any screen the setup can show."""

    def setUp(self):
        self.manager = make_manager()
        self.content = (
            '#define LED_DATA_PIN 3\n'
            '#define LED_NUM_LEDS 11\n'
            '#define BUTTON_PIN 4\n'
            '#define DISTANCE_SENSOR_SDA 6\n'
            '#define DISTANCE_SENSOR_SCL 7\n'
            '#define MESH_ON true\n'
            '#define ESPNOW_CHANNEL 1\n'
            f'#define GLOW_GROUP_KEY_HEX "{VALID_KEY}"\n'
            '#define GLOW_MAX_GROUP_NODES 8\n'
        )

    def test_key_value_listing_is_redacted(self):
        workflow = ConfigurationWorkflow(self.manager)
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            workflow._display_config_values(self.content)

        printed = output.getvalue()
        self.assertNotIn(VALID_KEY, printed)
        self.assertIn('Group Key', printed)
        self.assertIn(VALID_KEY[-8:], printed)

    def test_full_file_view_is_redacted(self):
        printed = self.manager.redact_config_content(self.content)
        self.assertNotIn(VALID_KEY, printed)


class FilePermissionTests(unittest.TestCase):
    def test_backup_is_owner_only(self):
        manager = make_manager()
        with tempfile.TemporaryDirectory() as temp_dir:
            manager.config_path = Path(temp_dir) / 'GlowConfig.h'
            manager.backup_dir = Path(temp_dir) / 'backups'
            manager.backup_dir.mkdir()
            manager.config_path.write_text('#define MESH_ON false\n')

            manager._create_backup()

            backup = next(manager.backup_dir.iterdir())
            self.assertEqual(stat.S_IMODE(backup.stat().st_mode), 0o600)

    def test_backup_directory_is_owner_only(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            manager = ConfigManager.__new__(ConfigManager)
            manager.config_path = Path(temp_dir) / 'GlowConfig.h'
            manager.template_path = Path(temp_dir) / 'GlowConfig.h-template'
            manager.backup_dir = Path(temp_dir) / 'backups'
            manager.validator = ConfigValidator()
            manager.backup_dir.mkdir(mode=0o700, exist_ok=True)
            os.chmod(manager.backup_dir, 0o700)

            self.assertEqual(stat.S_IMODE(manager.backup_dir.stat().st_mode), 0o700)

    def test_config_created_from_template_is_owner_only(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            manager = ConfigManager.__new__(ConfigManager)
            manager.config_path = Path(temp_dir) / 'GlowConfig.h'
            manager.template_path = Path(temp_dir) / 'GlowConfig.h-template'
            manager.backup_dir = Path(temp_dir) / 'backups'
            manager.backup_dir.mkdir()
            manager.validator = ConfigValidator()
            manager.template_path.write_text('#define MESH_ON false\n')
            manager.template_path.chmod(0o644)

            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertTrue(manager.create_from_template())

            self.assertEqual(
                stat.S_IMODE(manager.config_path.stat().st_mode), 0o600
            )


class TemplateValidationTests(unittest.TestCase):
    """The shipped template must be safe to compile straight from the repo."""

    def setUp(self):
        self.template = (PROJECT_ROOT / 'include' / 'GlowConfig.h-template').read_text()

    def test_repository_template_passes_the_checker(self):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            with patch('managers.git_manager.Path') as path_mock:
                path_mock.return_value.exists.return_value = True
                with patch('builtins.open', unittest.mock.mock_open(
                        read_data=self.template)):
                    self.assertTrue(ProjectStructureValidator.check_config_template())

    def test_repository_template_ships_without_a_usable_key(self):
        # Shipping a real key would make every clone share one group secret.
        self.assertIn(f'#define GLOW_GROUP_KEY_HEX "{TEMPLATE_PLACEHOLDER}"',
                      self.template)
        self.assertIn('#define MESH_ON false', self.template)
        self.assertIn('#define GLOW_SYNC_FOLLOW_DEFAULT true', self.template)
        self.assertIn('#define GLOW_SYNC_PUBLISH_DEFAULT true', self.template)
        self.assertIn('#define GLOW_PORTAL_ENABLED false', self.template)
        self.assertIn('#define GLOW_PORTAL_PASSWORD "PROVISION_WITH_SETUP"', self.template)
        self.assertIn('#define GLOW_OTA_ENABLED false', self.template)
        self.assertIn('#define GLOW_OTA_PASSWORD "PROVISION_WITH_SETUP"', self.template)

    def test_template_with_an_invalid_sync_default_is_rejected(self):
        broken = self.template.replace(
            '#define GLOW_SYNC_FOLLOW_DEFAULT true',
            '#define GLOW_SYNC_FOLLOW_DEFAULT sometimes',
        )
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            with patch('managers.git_manager.Path') as path_mock:
                path_mock.return_value.exists.return_value = True
                with patch('builtins.open', unittest.mock.mock_open(read_data=broken)):
                    self.assertFalse(ProjectStructureValidator.check_config_template())

    def test_template_with_mesh_enabled_and_placeholder_key_is_rejected(self):
        broken = self.template.replace('#define MESH_ON false', '#define MESH_ON true')
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            with patch('managers.git_manager.Path') as path_mock:
                path_mock.return_value.exists.return_value = True
                with patch('builtins.open', unittest.mock.mock_open(read_data=broken)):
                    self.assertFalse(ProjectStructureValidator.check_config_template())

    def test_template_with_an_unquoted_key_is_rejected(self):
        broken = self.template.replace(
            f'#define GLOW_GROUP_KEY_HEX "{TEMPLATE_PLACEHOLDER}"',
            f'#define GLOW_GROUP_KEY_HEX {VALID_KEY}',
        )
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            with patch('managers.git_manager.Path') as path_mock:
                path_mock.return_value.exists.return_value = True
                with patch('builtins.open', unittest.mock.mock_open(read_data=broken)):
                    self.assertFalse(ProjectStructureValidator.check_config_template())

    def test_template_with_a_bad_group_size_is_rejected(self):
        broken = self.template.replace(
            '#define GLOW_MAX_GROUP_NODES 8', '#define GLOW_MAX_GROUP_NODES 32'
        )
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            with patch('managers.git_manager.Path') as path_mock:
                path_mock.return_value.exists.return_value = True
                with patch('builtins.open', unittest.mock.mock_open(read_data=broken)):
                    self.assertFalse(ProjectStructureValidator.check_config_template())


class GitIgnoreTests(unittest.TestCase):
    def test_local_config_is_ignored(self):
        gitignore = (PROJECT_ROOT / '.gitignore').read_text()
        self.assertTrue(
            any(line.strip() in ('include/GlowConfig.h', 'GlowConfig.h')
                for line in gitignore.splitlines()),
            "GlowConfig.h must never be committed: it holds the group key",
        )


if __name__ == '__main__':
    unittest.main(verbosity=2)
