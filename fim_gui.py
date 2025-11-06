import os
import json
import hashlib
import time
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

# ============================
# Project setup (auto-creates folder)
# ============================
BASE_DIR = r"C:\fimf_project"
if not os.path.exists(BASE_DIR):
    os.makedirs(BASE_DIR)

BASELINE_FILE = os.path.join(BASE_DIR, "baseline.json")
LOG_FILE = os.path.join(BASE_DIR, "fim_log.txt")

# ============================
# Helper functions
# ============================

def sha256_of_file(path):
    """Compute SHA256 hash of a file."""
    try:
        with open(path, "rb") as f:
            return hashlib.sha256(f.read()).hexdigest()
    except Exception:
        return None


def build_baseline(directory):
    """Scan directory and return file-hash map."""
    baseline = {}
    for root, _, files in os.walk(directory):
        for file in files:
            full = os.path.join(root, file)
            h = sha256_of_file(full)
            if h:
                baseline[full] = h
    return baseline


def save_json(data, filename):
    with open(filename, "w") as f:
        json.dump(data, f, indent=2)


def load_json(filename):
    if os.path.exists(filename):
        with open(filename, "r") as f:
            return json.load(f)
    return {}


def log_change(message):
    with open(LOG_FILE, "a") as f:
        f.write(f"{time.strftime('%Y-%m-%d %H:%M:%S')} - {message}\n")


# ============================
# GUI
# ============================

class FIMApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Python File Integrity Monitor (FIM)")
        self.geometry("850x500")
        self.configure(bg="#0b132b")  # dark blue background
        self.resizable(False, False)

        style = ttk.Style()
        style.theme_use("clam")
        style.configure("TLabel", background="#0b132b", foreground="white", font=("Consolas", 10))
        style.configure("TButton", background="#1c2541", foreground="white", font=("Consolas", 10, "bold"))
        style.map("TButton", background=[("active", "#3a506b")])

        self.directory = tk.StringVar(value=BASE_DIR)
        self.baseline = {}

        self.create_widgets()

    def create_widgets(self):
        # Directory selection
        frm = ttk.Frame(self)
        frm.pack(pady=10)

        ttk.Label(frm, text="Directory to monitor:").pack(side=tk.LEFT, padx=5)
        ttk.Entry(frm, textvariable=self.directory, width=60).pack(side=tk.LEFT, padx=5)
        ttk.Button(frm, text="Browse", command=self.browse_dir).pack(side=tk.LEFT)

        # Buttons
        btn_frame = ttk.Frame(self)
        btn_frame.pack(pady=10)

        ttk.Button(btn_frame, text="Build Baseline", command=self.build_baseline_action).pack(side=tk.LEFT, padx=10)
        ttk.Button(btn_frame, text="Scan for Changes", command=self.scan_changes).pack(side=tk.LEFT, padx=10)

        # Log area (dark theme)
        self.log_box = tk.Text(self, wrap="word", width=100, height=20, bg="#1c2541", fg="white", insertbackground="white")
        self.log_box.pack(padx=10, pady=10)
        self.log_box.insert(tk.END, "Ready.\n")

        # Define tag colors
        self.log_box.tag_configure("created", foreground="#00ff7f")     # soft green
        self.log_box.tag_configure("deleted", foreground="#ff5c5c")     # soft red
        self.log_box.tag_configure("modified", foreground="#ffdd57")    # soft yellow
        self.log_box.tag_configure("normal", foreground="white")

    def browse_dir(self):
        path = filedialog.askdirectory(initialdir=BASE_DIR)
        if path:
            self.directory.set(path)

    def build_baseline_action(self):
        directory = self.directory.get()
        if not os.path.isdir(directory):
            messagebox.showerror("Error", "Invalid directory")
            return

        self.log("Building baseline, please wait...", "normal")
        self.baseline = build_baseline(directory)
        save_json(self.baseline, BASELINE_FILE)
        self.log(f"Baseline created with {len(self.baseline)} files.", "normal")
        log_change(f"Baseline created for directory: {directory}")

    def scan_changes(self):
        directory = self.directory.get()
        if not os.path.isdir(directory):
            messagebox.showerror("Error", "Invalid directory")
            return

        old_hashes = load_json(BASELINE_FILE)
        if not old_hashes:
            messagebox.showwarning("Warning", "No baseline found. Please build one first.")
            return

        self.log("Scanning for changes...", "normal")
        current_hashes = build_baseline(directory)

        added = [f for f in current_hashes if f not in old_hashes]
        deleted = [f for f in old_hashes if f not in current_hashes]
        modified = [f for f in current_hashes if f in old_hashes and current_hashes[f] != old_hashes[f]]

        if not added and not deleted and not modified:
            self.log("✅ No changes detected.", "normal")
            return

        for f in added:
            msg = f"[CREATED] {f}"
            self.log(msg, "created")
            log_change(msg)
        for f in deleted:
            msg = f"[DELETED] {f}"
            self.log(msg, "deleted")
            log_change(msg)
        for f in modified:
            msg = f"[MODIFIED] {f}"
            self.log(msg, "modified")
            log_change(msg)

        self.log("Scan complete. All changes logged.", "normal")
        save_json(current_hashes, BASELINE_FILE)

    def log(self, msg, tag="normal"):
        self.log_box.insert(tk.END, f"{msg}\n", tag)
        self.log_box.see(tk.END)


# ============================
# Run the app
# ============================

if __name__ == "__main__":
    app = FIMApp()
    app.mainloop()
