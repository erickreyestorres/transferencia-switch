"""Interfaz gráfica en español para el backend USB."""

from __future__ import annotations

from pathlib import Path
import queue
import threading
import time
import tkinter as tk
from tkinter import filedialog, messagebox, ttk
from typing import Any

from .server import BackendError, DbiBackendServer


def format_bytes(value: int) -> str:
    size = float(value)
    for unit in ("B", "KiB", "MiB", "GiB", "TiB"):
        if size < 1024 or unit == "TiB":
            return f"{size:.1f} {unit}" if unit != "B" else f"{int(size)} B"
        size /= 1024
    return f"{size:.1f} TiB"


def format_duration(seconds: float) -> str:
    seconds = max(0, int(seconds))
    hours, remainder = divmod(seconds, 3600)
    minutes, seconds = divmod(remainder, 60)
    return f"{hours:02d}:{minutes:02d}:{seconds:02d}"


class BackendApp:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("DBI Open Backend")
        self.root.geometry("920x620")
        self.root.minsize(760, 520)
        self.files: dict[str, Path] = {}
        self.progress: dict[str, int] = {}
        self.events: queue.Queue[dict[str, Any]] = queue.Queue()
        self.stop_event = threading.Event()
        self.worker: threading.Thread | None = None
        self.current_file: str | None = None
        self.session_started: float | None = None

        self._build_ui()
        self.root.protocol("WM_DELETE_WINDOW", self._close)
        self.root.after(100, self._poll_events)

    def _build_ui(self) -> None:
        toolbar = ttk.Frame(self.root, padding=10)
        toolbar.pack(fill=tk.X)
        self.add_files_button = ttk.Button(toolbar, text="Agregar archivos", command=self._add_files)
        self.add_files_button.pack(side=tk.LEFT, padx=(0, 6))
        self.add_folder_button = ttk.Button(toolbar, text="Agregar carpeta", command=self._add_folder)
        self.add_folder_button.pack(side=tk.LEFT, padx=6)
        self.clear_button = ttk.Button(toolbar, text="Limpiar cola", command=self._clear)
        self.clear_button.pack(side=tk.LEFT, padx=6)
        self.start_button = ttk.Button(toolbar, text="Iniciar servidor", command=self._start)
        self.start_button.pack(side=tk.RIGHT, padx=(6, 0))
        self.stop_button = ttk.Button(toolbar, text="Cancelar", command=self._stop, state=tk.DISABLED)
        self.stop_button.pack(side=tk.RIGHT, padx=6)

        columns = ("size", "status", "progress")
        self.tree = ttk.Treeview(self.root, columns=columns, show="tree headings", height=14)
        self.tree.heading("#0", text="Archivo")
        self.tree.heading("size", text="Tamaño")
        self.tree.heading("status", text="Estado")
        self.tree.heading("progress", text="Progreso")
        self.tree.column("#0", width=420, minwidth=220)
        self.tree.column("size", width=110, anchor=tk.E)
        self.tree.column("status", width=140)
        self.tree.column("progress", width=110, anchor=tk.E)
        self.tree.pack(fill=tk.BOTH, expand=True, padx=10)

        progress_frame = ttk.LabelFrame(self.root, text="Transferencia", padding=10)
        progress_frame.pack(fill=tk.X, padx=10, pady=10)
        self.current_label = ttk.Label(progress_frame, text="Archivo actual: —")
        self.current_label.pack(anchor=tk.W)
        self.current_bar = ttk.Progressbar(progress_frame, maximum=100)
        self.current_bar.pack(fill=tk.X, pady=(4, 8))
        self.total_label = ttk.Label(progress_frame, text="Progreso total: 0 B / 0 B")
        self.total_label.pack(anchor=tk.W)
        self.total_bar = ttk.Progressbar(progress_frame, maximum=100)
        self.total_bar.pack(fill=tk.X, pady=(4, 8))
        self.speed_label = ttk.Label(progress_frame, text="Velocidad: —")
        self.speed_label.pack(anchor=tk.W)

        self.status = tk.StringVar(value="Agrega archivos para comenzar.")
        ttk.Label(self.root, textvariable=self.status, relief=tk.SUNKEN, anchor=tk.W, padding=6).pack(
            fill=tk.X, side=tk.BOTTOM
        )

    def _add_path(self, path: Path) -> None:
        path = path.resolve()
        if not path.is_file():
            return
        for existing in self.files.values():
            if existing.name == path.name and existing != path:
                messagebox.showwarning(
                    "Nombre duplicado",
                    f"No se agregó {path.name}: ya existe otro archivo con ese nombre.",
                )
                return
        key = str(path)
        if key in self.files:
            return
        self.files[key] = path
        self.progress[key] = 0
        self.tree.insert("", tk.END, iid=key, text=path.name, values=(format_bytes(path.stat().st_size), "Pendiente", "0 %"))
        self._refresh_total()

    def _add_files(self) -> None:
        for filename in filedialog.askopenfilenames(title="Selecciona archivos para DBI"):
            self._add_path(Path(filename))

    def _add_folder(self) -> None:
        folder = filedialog.askdirectory(title="Selecciona una carpeta")
        if folder:
            for path in sorted(Path(folder).iterdir()):
                self._add_path(path)

    def _clear(self) -> None:
        self.files.clear()
        self.progress.clear()
        for item in self.tree.get_children():
            self.tree.delete(item)
        self.current_bar["value"] = 0
        self.total_bar["value"] = 0
        self._refresh_total()

    def _set_running(self, running: bool) -> None:
        normal = tk.DISABLED if running else tk.NORMAL
        self.add_files_button["state"] = normal
        self.add_folder_button["state"] = normal
        self.clear_button["state"] = normal
        self.start_button["state"] = normal
        self.stop_button["state"] = tk.NORMAL if running else tk.DISABLED

    def _start(self) -> None:
        if not self.files:
            messagebox.showinfo("Cola vacía", "Agrega al menos un archivo antes de iniciar.")
            return
        self.stop_event = threading.Event()
        self.session_started = time.monotonic()
        for key in self.files:
            self.progress[key] = 0
            path = self.files[key]
            self.tree.item(key, values=(format_bytes(path.stat().st_size), "Pendiente", "0 %"))
        self._set_running(True)
        try:
            server = DbiBackendServer(self.files.values(), self.events.put, self.stop_event)
        except BackendError as exc:
            self._set_running(False)
            messagebox.showerror("No se pudo iniciar", str(exc))
            return
        self.worker = threading.Thread(target=self._serve, args=(server,), daemon=True)
        self.worker.start()

    def _serve(self, server: DbiBackendServer) -> None:
        try:
            server.serve()
        except Exception:
            pass
        finally:
            self.events.put({"type": "worker_stopped"})

    def _stop(self) -> None:
        self.stop_event.set()
        self.status.set("Cancelando la transferencia…")

    def _file_key(self, name: str) -> str | None:
        return next((key for key, path in self.files.items() if path.name == name), None)

    def _handle_event(self, event: dict[str, Any]) -> None:
        event_type = event["type"]
        if event_type == "status":
            self.status.set(event["message"])
        elif event_type == "file_started":
            name = event["name"]
            self.current_file = name
            self.current_label.config(text=f"Archivo actual: {name}")
            key = self._file_key(name)
            if key:
                self.tree.item(key, values=(format_bytes(event["size"]), "Transfiriendo", "0 %"))
        elif event_type == "file_progress":
            name = event["name"]
            key = self._file_key(name)
            if not key:
                return
            self.progress[key] = event["transferred"]
            fraction = event["transferred"] / event["size"] if event["size"] else 1.0
            status = "Enviado" if event["complete"] else "Transfiriendo"
            self.tree.item(
                key,
                values=(format_bytes(event["size"]), status, f"{fraction * 100:.1f} %"),
            )
            self.current_bar["value"] = fraction * 100
            self._refresh_total()
            elapsed = time.monotonic() - self.session_started if self.session_started else 0.0
            total = sum(path.stat().st_size for path in self.files.values())
            transferred = sum(self.progress.values())
            speed = float(event["speed"])
            remaining = (total - transferred) / speed if speed > 0 else 0.0
            self.speed_label.config(
                text=(
                    f"Velocidad: {format_bytes(int(speed))}/s  |  "
                    f"Transcurrido: {format_duration(elapsed)}  |  "
                    f"Restante estimado: {format_duration(remaining)}"
                )
            )
        elif event_type == "session_finished":
            sent = sum(item["status"] == "enviado" for item in event["files"])
            partial = sum(item["status"] == "parcial" for item in event["files"])
            untouched = sum(item["status"] == "no_solicitado" for item in event["files"])
            elapsed = time.monotonic() - self.session_started if self.session_started else 0.0
            transferred = sum(item["transferred"] for item in event["files"])
            messagebox.showinfo(
                "Resumen de transferencia",
                "La sesión USB terminó.\n\n"
                f"Enviados completamente: {sent}\n"
                f"Transferencia parcial: {partial}\n"
                f"No solicitados por DBI: {untouched}\n\n"
                f"Datos enviados: {format_bytes(transferred)}\n"
                f"Duración: {format_duration(elapsed)}\n\n"
                "Este resumen confirma datos enviados, no la instalación final del contenido.",
            )
            self.status.set("Transferencia finalizada.")
        elif event_type == "error":
            self.status.set("La transferencia terminó con un error.")
            messagebox.showerror("Error de transferencia", event["message"])
        elif event_type == "worker_stopped":
            self._set_running(False)

    def _refresh_total(self) -> None:
        total = sum(path.stat().st_size for path in self.files.values())
        transferred = sum(self.progress.values())
        fraction = transferred / total if total else 0.0
        self.total_bar["value"] = fraction * 100
        self.total_label.config(text=f"Progreso total: {format_bytes(transferred)} / {format_bytes(total)}")
        if not self.files:
            self.status.set("Agrega archivos para comenzar.")

    def _poll_events(self) -> None:
        while True:
            try:
                self._handle_event(self.events.get_nowait())
            except queue.Empty:
                break
        self.root.after(100, self._poll_events)

    def _close(self) -> None:
        self.stop_event.set()
        self.root.destroy()


def run() -> None:
    root = tk.Tk()
    BackendApp(root)
    root.mainloop()
