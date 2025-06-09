"""
Progress Bar Module

Provides visual progress indicators for long-running operations.
"""

import sys
import time
import threading


class ProgressBar:
    """Animated progress bar for terminal applications."""
    
    def __init__(self, total=100, width=50, char="█", empty_char="░"):
        """Initialize progress bar.
        
        Args:
            total: Total number of steps
            width: Width of progress bar in characters
            char: Character to use for filled portion
            empty_char: Character to use for empty portion
        """
        self.total = total
        self.width = width
        self.char = char
        self.empty_char = empty_char
        self.current = 0
        
    def update(self, current, message=""):
        """Update progress bar.
        
        Args:
            current: Current progress value
            message: Optional message to display
        """
        self.current = current
        self._render(message)
        
    def _render(self, message=""):
        """Render the progress bar."""
        percentage = min(100, (self.current / self.total) * 100)
        filled_width = int((self.current / self.total) * self.width)
        empty_width = self.width - filled_width
        
        bar = self.char * filled_width + self.empty_char * empty_width
        
        # Create the progress line
        progress_line = f"📊 [{bar}] {percentage:6.1f}% {message}"
        
        # Clear the entire line and print progress
        sys.stdout.write('\r')
        sys.stdout.write('\033[K')  # Clear to end of line
        sys.stdout.write(progress_line)
        sys.stdout.flush()
        
    def __enter__(self):
        """Context manager entry."""
        return self
        
    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit."""
        # Print newline to finish the progress bar
        print()
        return False
        
    def set_status(self, message):
        """Set status message without changing progress.
        
        Args:
            message: Status message to display
        """
        self._render(message)
        
    def finish(self, message="Complete!"):
        """Finish the progress bar with success message."""
        self.current = self.total
        self._render(message)
        # Move to next line to finish the progress bar line
        sys.stdout.write('\n')
        sys.stdout.flush()


class SpinnerProgress:
    """Animated spinner for indeterminate progress."""
    
    SPINNER_CHARS = ['⠋', '⠙', '⠹', '⠸', '⠼', '⠴', '⠦', '⠧', '⠇', '⠏']
    
    def __init__(self, message="Processing..."):
        """Initialize spinner.
        
        Args:
            message: Message to display with spinner
        """
        self.message = message
        self.spinning = False
        self.thread = None
        
    def start(self):
        """Start the spinner animation."""
        self.spinning = True
        self.thread = threading.Thread(target=self._spin)
        self.thread.daemon = True
        self.thread.start()
        
    def stop(self, success_message=""):
        """Stop the spinner animation.
        
        Args:
            success_message: Optional success message to display
        """
        self.spinning = False
        if self.thread:
            self.thread.join()
            
        # Clear spinner line
        sys.stdout.write('\r' + ' ' * (len(self.message) + 10) + '\r')
        
        if success_message:
            print(f"✅ {success_message}")
            
    def __enter__(self):
        """Context manager entry."""
        self.start()
        return self
        
    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit."""
        if exc_type is None:
            self.stop("Completed")
        else:
            self.stop("Failed")
        return False
            
    def _spin(self):
        """Internal spinning animation."""
        i = 0
        while self.spinning:
            char = self.SPINNER_CHARS[i % len(self.SPINNER_CHARS)]
            sys.stdout.write(f'\r{char} {self.message}')
            sys.stdout.flush()
            time.sleep(0.1)
            i += 1


class TaskProgress:
    """Multi-step task progress tracker."""
    
    def __init__(self, tasks):
        """Initialize task progress.
        
        Args:
            tasks: List of task names
        """
        self.tasks = tasks
        self.current_task = 0
        self.total_tasks = len(tasks)
        
    def start_task(self, task_index, message=""):
        """Start a specific task.
        
        Args:
            task_index: Index of task to start
            message: Optional custom message
        """
        self.current_task = task_index
        task_name = self.tasks[task_index]
        display_message = message or f"Executing {task_name}..."
        
        print(f"\n📋 Step {task_index + 1}/{self.total_tasks}: {display_message}")
        
    def complete_task(self, task_index, success_message=""):
        """Complete a specific task.
        
        Args:
            task_index: Index of completed task
            success_message: Optional success message
        """
        task_name = self.tasks[task_index]
        message = success_message or f"{task_name} completed"
        print(f"✅ {message}")
        
    def finish_all(self):
        """Finish all tasks with summary."""
        print(f"\n🎉 All tasks completed successfully! ({self.total_tasks}/{self.total_tasks})")
        
    def __enter__(self):
        """Context manager entry."""
        return self
        
    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit."""
        if exc_type is None:
            self.finish_all()
        return False


def demo_progress():
    """Demo function to show progress bar examples."""
    print("📊 Progress Bar Demo")
    print("=" * 50)
    
    # Regular progress bar
    print("\n1. Regular Progress Bar:")
    progress = ProgressBar(total=10, width=30)
    for i in range(11):
        progress.update(i, f"Processing item {i}")
        time.sleep(0.2)
        
    # Spinner demo
    print("\n2. Spinner Progress:")
    spinner = SpinnerProgress("Downloading packages...")
    spinner.start()
    time.sleep(3)
    spinner.stop("Download complete!")
    
    # Task progress demo
    print("\n3. Task Progress:")
    tasks = ["Initialize", "Configure", "Build", "Flash"]
    task_progress = TaskProgress(tasks)
    
    for i, task in enumerate(tasks):
        task_progress.start_task(i)
        time.sleep(1)
        task_progress.complete_task(i)
        
    task_progress.finish_all()


if __name__ == "__main__":
    demo_progress()
