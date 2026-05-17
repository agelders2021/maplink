#!/usr/bin/env python3
"""
MapLink - Generate Google Maps email links with directions and a CalTopo link.
Copies formatted text to clipboard, ready to paste into an email.
"""

import tkinter as tk
from tkinter import messagebox
import re


# ── Color palette ──────────────────────────────────────────────────────────────
BG       = "#1e2330"
PANEL    = "#262d3d"
ACCENT   = "#4a9eff"
ACCENT2  = "#2d6bbf"
TEXT     = "#e8eaf0"
SUBTEXT  = "#8a93a8"
ENTRY_BG = "#2e3748"
ENTRY_BD = "#3d4a63"
SUCCESS  = "#3ecf8e"
MENU_BG  = "#2e3748"
MENU_FG  = "#e8eaf0"


def parse_coords(raw: str):
    parts = re.split(r",\s*", raw.strip())
    if len(parts) != 2:
        raise ValueError("Enter coordinates as:  lat, lon")
    lat, lon = parts
    float(lat)
    float(lon)
    return lat.strip(), lon.strip()


def build_output(name: str, lat: str, lon: str, caltopo: str, directions: str) -> str:
    gmaps_url = f"https://www.google.com/maps/search/?api=1&query={lat}%2C{lon}"

    lines = []
    if name:
        lines.append(f"Location: {name}")
    lines.append(f"Google Maps: {gmaps_url}")
    if caltopo.strip():
        lines.append(f"CalTopo:     {caltopo.strip()}")
    if directions.strip():
        lines.append("")
        lines.append("Directions:")
        lines.append(directions.strip())
    return "\n".join(lines)


# ── Right-click context menu ───────────────────────────────────────────────────

def _attach_context_menu(widget):
    is_text = isinstance(widget, tk.Text)

    menu = tk.Menu(
        widget, tearoff=0,
        bg=MENU_BG, fg=MENU_FG,
        activebackground=ACCENT, activeforeground="#ffffff",
        relief="flat", bd=1,
        font=("Segoe UI", 9)
    )

    menu.add_command(label="Cut",   command=lambda: widget.event_generate("<<Cut>>"))
    menu.add_command(label="Copy",  command=lambda: widget.event_generate("<<Copy>>"))
    menu.add_command(label="Paste", command=lambda: widget.event_generate("<<Paste>>"))
    menu.add_separator()

    def select_all():
        if is_text:
            widget.tag_add("sel", "1.0", "end")
        else:
            widget.select_range(0, "end")
            widget.icursor("end")

    menu.add_command(label="Select All", command=select_all)

    def show_menu(event):
        widget.focus_set()
        try:
            menu.tk_popup(event.x_root, event.y_root)
        finally:
            menu.grab_release()

    widget.bind("<Button-3>", show_menu)


# ── Main application ───────────────────────────────────────────────────────────

class MapLinkApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("MapLink")
        self.resizable(False, False)
        self.configure(bg=BG)

        w, h = 520, 540
        self.geometry(f"{w}x{h}+{(self.winfo_screenwidth()-w)//2}+{(self.winfo_screenheight()-h)//2}")

        self._build_ui()

    def _build_ui(self):
        # ── Header ─────────────────────────────────────────────────────────────
        header = tk.Frame(self, bg=PANEL, height=64)
        header.pack(fill="x")
        header.pack_propagate(False)

        tk.Label(
            header, text="📍 MapLink",
            font=("Segoe UI", 18, "bold"),
            bg=PANEL, fg=ACCENT
        ).pack(side="left", padx=24, pady=14)

        tk.Label(
            header, text="Generate tappable map links for email",
            font=("Segoe UI", 9),
            bg=PANEL, fg=SUBTEXT
        ).pack(side="left", pady=20)

        # ── Form area ──────────────────────────────────────────────────────────
        form = tk.Frame(self, bg=BG)
        form.pack(fill="both", expand=True, padx=24, pady=20)

        self._label(form, "Location Name")
        self.name_var = tk.StringVar()
        self._entry(form, self.name_var, "e.g. Trailhead Parking")

        self._spacer(form)

        self._label(form, "Coordinates  (lat, lon)")
        self.coords_var = tk.StringVar()
        self._entry(form, self.coords_var, "e.g. 35.09742, -106.33096")

        self._spacer(form)

        self._label(form, "CalTopo Link  (paste your existing map URL)")
        self.caltopo_var = tk.StringVar()
        self._entry(form, self.caltopo_var, "e.g. https://caltopo.com/m/XXXX")

        self._spacer(form)

        self._label(form, "Directions / Notes")
        self.directions_text = tk.Text(
            form,
            height=6,
            font=("Segoe UI", 10),
            bg=ENTRY_BG, fg=TEXT,
            insertbackground=ACCENT,
            relief="flat", bd=0,
            highlightthickness=1,
            highlightbackground=ENTRY_BD,
            highlightcolor=ACCENT,
            padx=10, pady=8,
            wrap="word"
        )
        self.directions_text.pack(fill="x", ipady=2)
        self._placeholder(self.directions_text, "Optional turn-by-turn notes or landmarks…")
        _attach_context_menu(self.directions_text)

        # ── Button + status ────────────────────────────────────────────────────
        bottom = tk.Frame(self, bg=BG)
        bottom.pack(fill="x", padx=24, pady=(0, 20))

        self.status_var = tk.StringVar()
        self.status_lbl = tk.Label(
            bottom, textvariable=self.status_var,
            font=("Segoe UI", 9), bg=BG, fg=SUBTEXT
        )
        self.status_lbl.pack(side="left")

        self.btn = tk.Button(
            bottom,
            text="Copy to Clipboard  ⧉",
            font=("Segoe UI", 10, "bold"),
            bg=ACCENT, fg="#ffffff",
            activebackground=ACCENT2, activeforeground="#ffffff",
            relief="flat", bd=0,
            padx=20, pady=10,
            cursor="hand2",
            command=self._generate
        )
        self.btn.pack(side="right")
        self.btn.bind("<Enter>", lambda e: self.btn.config(bg=ACCENT2))
        self.btn.bind("<Leave>", lambda e: self.btn.config(bg=ACCENT))

    def _label(self, parent, text):
        tk.Label(
            parent, text=text,
            font=("Segoe UI", 9, "bold"),
            bg=BG, fg=SUBTEXT, anchor="w"
        ).pack(fill="x", pady=(0, 4))

    def _entry(self, parent, var, placeholder):
        e = tk.Entry(
            parent, textvariable=var,
            font=("Segoe UI", 11),
            bg=ENTRY_BG, fg=TEXT,
            insertbackground=ACCENT,
            relief="flat", bd=0,
            highlightthickness=1,
            highlightbackground=ENTRY_BD,
            highlightcolor=ACCENT,
        )
        e.pack(fill="x", ipady=8)

        e.insert(0, placeholder)
        e.config(fg=SUBTEXT)

        def on_focus_in(ev, entry=e, ph=placeholder):
            if entry.get() == ph:
                entry.delete(0, "end")
                entry.config(fg=TEXT)

        def on_focus_out(ev, entry=e, ph=placeholder):
            if not entry.get():
                entry.insert(0, ph)
                entry.config(fg=SUBTEXT)

        e.bind("<FocusIn>", on_focus_in)
        e.bind("<FocusOut>", on_focus_out)
        _attach_context_menu(e)

    def _spacer(self, parent, h=12):
        tk.Frame(parent, bg=BG, height=h).pack(fill="x")

    def _placeholder(self, text_widget, ph):
        text_widget.insert("1.0", ph)
        text_widget.config(fg=SUBTEXT)

        def on_focus_in(ev):
            if text_widget.get("1.0", "end-1c") == ph:
                text_widget.delete("1.0", "end")
                text_widget.config(fg=TEXT)

        def on_focus_out(ev):
            if not text_widget.get("1.0", "end-1c").strip():
                text_widget.insert("1.0", ph)
                text_widget.config(fg=SUBTEXT)

        text_widget.bind("<FocusIn>", on_focus_in)
        text_widget.bind("<FocusOut>", on_focus_out)

    def _get_directions(self):
        ph = "Optional turn-by-turn notes or landmarks…"
        raw = self.directions_text.get("1.0", "end-1c")
        return "" if raw == ph else raw

    def _generate(self):
        name     = self.name_var.get().strip()
        coords   = self.coords_var.get().strip()
        caltopo  = self.caltopo_var.get().strip()
        dirs     = self._get_directions()

        if name == "e.g. Trailhead Parking":
            name = ""
        if coords == "e.g. 35.09742, -106.33096":
            coords = ""
        if caltopo == "e.g. https://caltopo.com/m/XXXX":
            caltopo = ""

        if not coords:
            messagebox.showerror("Missing coordinates", "Please enter coordinates before copying.")
            return

        try:
            lat, lon = parse_coords(coords)
        except ValueError as exc:
            messagebox.showerror("Invalid coordinates", str(exc))
            return

        output = build_output(name, lat, lon, caltopo, dirs)

        self.clipboard_clear()
        self.clipboard_append(output)
        self.update()

        self.status_var.set("✓  Copied to clipboard!")
        self.status_lbl.config(fg=SUCCESS)
        self.after(3000, lambda: (self.status_var.set(""), self.status_lbl.config(fg=SUBTEXT)))


if __name__ == "__main__":
    app = MapLinkApp()
    app.mainloop()
