import sys
import re
import time
import serial
import serial.tools.list_ports
import numpy as np
from PyQt6.QtWidgets import (
    QApplication, QWidget, QHBoxLayout, QVBoxLayout, QLabel, QSizePolicy,
    QDialog, QComboBox, QLineEdit, QPushButton, QMessageBox, QFormLayout
)
from PyQt6.QtCore import QTimer, Qt
import pyqtgraph.opengl as gl
import pyqtgraph as pg

# =====================================
# PRESETS  (SIGMA -50%, amplitud -30%)
# =====================================
PRESET = "DRAMATIC"

if PRESET == "SOFT":
    AMP_GAIN, MAX_AMP_PER_EVENT = 0.63, 38.5
    MIN_EVENT_DELTA = 2.0
    SIGMA, K_OSC_WAVELEN = 8.25, 20.0
elif PRESET == "MEDIUM":
    AMP_GAIN, MAX_AMP_PER_EVENT = 0.91, 56.0
    MIN_EVENT_DELTA = 1.5
    SIGMA, K_OSC_WAVELEN = 7.5, 20.0
else:  # DRAMATIC
    AMP_GAIN, MAX_AMP_PER_EVENT = 1.12, 70.0
    MIN_EVENT_DELTA = 1.0
    SIGMA, K_OSC_WAVELEN = 6.75, 20.0

# =====================================
# CONFIG
# =====================================
BAUDRATE = 115200

POOL_W = 800
POOL_D = 400
GRID_X = 110
GRID_Y = 50

HISTORY_SECONDS = 8.0

X_START = +340.0
X_END   = -340.0
V       = (X_START - X_END) / HISTORY_SECONDS

W_HALF      = 3.0 * SIGMA
TUKEY_ALPHA = 0.10

FADE_IN_TIME  = 0.06
FADE_OUT_TIME = 0.06

K_OSC = 2 * np.pi / K_OSC_WAVELEN

CROSS_RIPPLE = 0.03

WARN_THRESHOLD  = 125.0
ERROR_THRESHOLD = 175.0

COLOR_OK         = np.array([0.15, 0.85, 0.30])
COLOR_WARNING    = np.array([1.00, 0.85, 0.10])
COLOR_MAGNETIZED = np.array([1.00, 0.15, 0.15])
MAG_PULSE_FREQ   = 6.0

TINT_X = np.array([0.12, 0.18, 0.28])
TINT_Y = np.array([0.12, 0.22, 0.18])
TINT_Z = np.array([0.22, 0.14, 0.18])

BG_COLOR = pg.mkColor(4, 10, 26, 255)

# =====================================
# DIÁLOGO DE ARRANQUE (SO + puerto)
# =====================================
def ask_system_and_port():
    """
    Muestra un diálogo para elegir SO y puerto.
    Devuelve (serial_instance, system_label, port_name) o (None, ...) si se cancela.
    """
    dlg = QDialog()
    dlg.setWindowTitle("MagSenseUI — Configuración inicial")
    dlg.setModal(True)
    dlg.setMinimumWidth(460)

    layout = QVBoxLayout(dlg)
    layout.setContentsMargins(18, 18, 18, 18)
    layout.setSpacing(10)

    info = QLabel(
        "<b>Selecciona el sistema y el puerto serie</b><br>"
        "<span style='color:#888;'>El puerto es el que el ESP32 expone al conectarlo por USB.</span>"
    )
    layout.addWidget(info)

    form = QFormLayout()
    form.setSpacing(8)

    # --- Selector SO ---
    cb_os = QComboBox()
    cb_os.addItems(["Linux", "Windows"])
    # Default según plataforma real
    if sys.platform.startswith("win"):
        cb_os.setCurrentText("Windows")
    else:
        cb_os.setCurrentText("Linux")
    form.addRow("Sistema operativo:", cb_os)

    # --- Combo con puertos detectados + campo editable ---
    cb_port = QComboBox()
    cb_port.setEditable(True)
    form.addRow("Puerto serie:", cb_port)

    def refresh_ports():
        """Re-escanea puertos y actualiza las sugerencias del combo."""
        cb_port.clear()
        detected = [p.device for p in serial.tools.list_ports.comports()]
        os_now = cb_os.currentText()
        defaults = ["/dev/ttyACM0", "/dev/ttyUSB0"] if os_now == "Linux" else ["COM5", "COM3", "COM4"]
        items = detected if detected else defaults
        for it in items:
            cb_port.addItem(it)
        # deja seleccionado el primero
        if items:
            cb_port.setCurrentText(items[0])

    cb_os.currentTextChanged.connect(lambda _: refresh_ports())
    refresh_ports()

    # Botón para re-escanear puertos manualmente
    btn_scan = QPushButton("🔄 Re-escanear puertos")
    btn_scan.clicked.connect(refresh_ports)
    form.addRow("", btn_scan)

    layout.addLayout(form)

    # --- Botonera ---
    btn_box = QHBoxLayout()
    btn_ok = QPushButton("Conectar")
    btn_cancel = QPushButton("Cancelar")
    btn_ok.setDefault(True)
    btn_box.addStretch(1)
    btn_box.addWidget(btn_ok)
    btn_box.addWidget(btn_cancel)
    layout.addLayout(btn_box)

    result = {"ser": None, "system": None, "port": None}

    def try_connect():
        os_label = cb_os.currentText()
        port = cb_port.currentText().strip()
        if not port:
            QMessageBox.warning(dlg, "Puerto vacío", "Introduce un puerto serie.")
            return
        print(f"Abriendo puerto: {port}  ({os_label})")
        try:
            s = serial.Serial(port, BAUDRATE, timeout=0.02)
            result["ser"] = s
            result["system"] = os_label
            result["port"] = port
            dlg.accept()
        except Exception as e:
            QMessageBox.critical(
                dlg, "No se pudo abrir el puerto",
                f"Puerto: {port}\n\nError:\n{e}\n\n"
                f"Comprueba que:\n"
                f"• El ESP32 está conectado por USB\n"
                f"• En Linux tienes permisos (grupo 'dialout')\n"
                f"• El puerto no está siendo usado por otro programa"
            )

    btn_ok.clicked.connect(try_connect)
    btn_cancel.clicked.connect(dlg.reject)

    if dlg.exec() != QDialog.DialogCode.Accepted:
        return None, None, None
    return result["ser"], result["system"], result["port"]

# =====================================
# APP + PREGUNTA INICIAL
# =====================================
app = QApplication(sys.argv)

ser, SYSTEM_LABEL, PORT_NAME = ask_system_and_port()
if ser is None:
    print("Cancelado por el usuario.")
    sys.exit(0)

print(f"Puerto abierto correctamente: {PORT_NAME}  ({SYSTEM_LABEL})")

# =====================================
# VENTANA PRINCIPAL
# =====================================
main_window = QWidget()
main_window.setWindowTitle(
    f"MagSenseUI 3D — 3 vistas  [{PRESET}]  [{SYSTEM_LABEL} · {PORT_NAME}]"
)
main_window.resize(1100, 1400)

outer_layout = QVBoxLayout(main_window)
outer_layout.setContentsMargins(6, 6, 6, 6)
outer_layout.setSpacing(4)

# HUD arriba
hud = QLabel("X: --  Y: --  Z: --")
hud.setTextFormat(Qt.TextFormat.RichText)
hud.setStyleSheet("""
    QLabel {
        color: #7fe7ff;
        background: rgba(0,20,40,220);
        border: 1px solid #2a6fff;
        border-radius: 8px;
        padding: 8px 12px;
        font-family: 'Consolas', monospace;
        font-size: 13px;
    }
""")
outer_layout.addWidget(hud)

# Columna con las 3 vistas apiladas
views_layout = QVBoxLayout()
views_layout.setSpacing(6)
outer_layout.addLayout(views_layout, stretch=1)

# =====================================
# CREAR 3 GLViewWidget
# =====================================
def make_view(title_text):
    container = QWidget()
    vbox = QVBoxLayout(container)
    vbox.setContentsMargins(0, 0, 0, 0)
    vbox.setSpacing(0)

    label = QLabel(title_text)
    label.setAlignment(Qt.AlignmentFlag.AlignCenter)
    label.setStyleSheet("""
        QLabel {
            color: #cfe3ff;
            background: rgba(0,10,30,200);
            border: 1px solid #1e3a6a;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            padding: 3px;
            font-family: 'Consolas', monospace;
            font-size: 11px;
            font-weight: bold;
        }
    """)
    vbox.addWidget(label)

    view = gl.GLViewWidget()
    view.setBackgroundColor(BG_COLOR)
    view.setCameraPosition(distance=750, elevation=22, azimuth=18)
    view.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
    vbox.addWidget(view, stretch=1)

    return container, view

container_x, view_x = make_view("EJE X")
container_y, view_y = make_view("EJE Y")
container_z, view_z = make_view("EJE Z")

views_layout.addWidget(container_x, stretch=1)
views_layout.addWidget(container_y, stretch=1)
views_layout.addWidget(container_z, stretch=1)

# =====================================
# SINCRONIZACIÓN DE CÁMARAS
# =====================================
views = {'x': view_x, 'y': view_y, 'z': view_z}
last_cam = {'x': None, 'y': None, 'z': None}

def camera_signature(v):
    return (
        float(v.opts['distance']),
        float(v.opts['elevation']),
        float(v.opts['azimuth']),
        tuple(v.opts['center']),
    )

# =====================================
# MALLA
# =====================================
xx = np.linspace(-POOL_W/2, POOL_W/2, GRID_X)
yy = np.linspace(-POOL_D/2, POOL_D/2, GRID_Y)
XX, YY = np.meshgrid(xx, yy)

faces = []
for j in range(GRID_Y - 1):
    for i in range(GRID_X - 1):
        a = j * GRID_X + i
        b = a + 1
        c = a + GRID_X
        d = c + 1
        faces.append([a, b, c])
        faces.append([b, d, c])
faces = np.array(faces, dtype=np.uint32)

def make_plane(view, tint):
    verts = np.zeros((GRID_X * GRID_Y, 3), dtype=float)
    verts[:, 0] = XX.ravel()
    verts[:, 1] = YY.ravel()
    verts[:, 2] = 0.0

    colors = np.zeros((GRID_X * GRID_Y, 4), dtype=float)
    colors[:, 0] = tint[0]
    colors[:, 1] = tint[1]
    colors[:, 2] = tint[2]
    colors[:, 3] = 0.55

    mesh = gl.MeshData(vertexes=verts, faces=faces)
    item = gl.GLMeshItem(
        meshdata=mesh,
        smooth=True,
        shader='shaded',
        glOptions='additive',
        drawEdges=False
    )
    view.addItem(item)
    return item, verts

plane_x, verts_x = make_plane(view_x, TINT_X)
plane_y, verts_y = make_plane(view_y, TINT_Y)
plane_z, verts_z = make_plane(view_z, TINT_Z)

planes = {
    'x': {'view': view_x, 'item': plane_x, 'verts': verts_x, 'events': [], 'tint': TINT_X},
    'y': {'view': view_y, 'item': plane_y, 'verts': verts_y, 'events': [], 'tint': TINT_Y},
    'z': {'view': view_z, 'item': plane_z, 'verts': verts_z, 'events': [], 'tint': TINT_Z},
}

# =====================================
# ESTADO
# =====================================
prev   = None
last_t = time.time()

# =====================================
# HELPERS
# =====================================
def color_for_delta(delta):
    if delta > ERROR_THRESHOLD: return COLOR_MAGNETIZED
    if delta > WARN_THRESHOLD:  return COLOR_WARNING
    return COLOR_OK

def status_info(delta):
    if delta > ERROR_THRESHOLD: return "MAGNETIZED", "#ff4040"
    if delta > WARN_THRESHOLD:  return "WARNING",   "#ffe012"
    return "OK", "#3fdd66"

def tukey_window(u, alpha=TUKEY_ALPHA):
    absu = np.abs(u)
    smooth = 0.5 * (1 + np.cos(np.pi * (absu - (1 - alpha)) / alpha))
    return np.where(absu <= 1 - alpha, 1.0,
                    np.where(absu < 1, smooth, 0.0))

# =====================================
# UPDATE
# =====================================
def update():
    global prev, last_t
    try:
        now = time.time()
        last_t = now

        # ---- sincronizar cámaras ----
        for name, v in views.items():
            sig = camera_signature(v)
            if last_cam[name] is None:
                last_cam[name] = sig
            elif sig != last_cam[name]:
                last_cam[name] = sig
                for other_name, other_v in views.items():
                    if other_v is v: continue
                    if camera_signature(other_v) == sig:
                        continue
                    other_v.setCameraPosition(
                        pos=v.opts['center'],
                        distance=v.opts['distance'],
                        elevation=v.opts['elevation'],
                        azimuth=v.opts['azimuth'],
                    )
                    last_cam[other_name] = sig
                break

        # ---- leer serie ----
        line = ser.readline().decode(errors="ignore").strip()
        if line:
            m = re.search(
                r'X:\s([-\d.]+)\suT\sY:\s([-\d.]+)\suT\sZ:\s([-\d.]+)\suT',
                line
            )
            if m:
                x = float(m.group(1)); y = float(m.group(2)); z = float(m.group(3))
                cur = np.array([x, y, z])
                if prev is not None:
                    dv = cur - prev
                    d_abs = np.abs(dv)
                    delta_max = float(np.max(d_abs))

                    for idx, axis in enumerate(['x', 'y', 'z']):
                        d_axis = float(d_abs[idx])
                        if d_axis >= MIN_EVENT_DELTA:
                            planes[axis]['events'].append({
                                't':     now,
                                'delta': d_axis,
                                'color': color_for_delta(d_axis).copy()
                            })

                    status, sc = status_info(delta_max)
                    counts = "  ".join(
                        f"<span style='color:#3fdd66;'>{a.upper()}:{len(planes[a]['events'])}</span>"
                        for a in ['x', 'y', 'z']
                    )
                    hud.setText(
                        f"X: {x:7.2f}   Y: {y:7.2f}   Z: {z:7.2f} µT   │   "
                        f"Δx={d_abs[0]:5.1f}  Δy={d_abs[1]:5.1f}  Δz={d_abs[2]:5.1f}   │   "
                        f"<span style='color:{sc};'><b>{status}</b></span><br>"
                        f"eventos activos: {counts}   │   "
                        f"<span style='color:#3fdd66;'>■</span> OK  "
                        f"<span style='color:#ffe012;'>■</span> WARNING  "
                        f"<span style='color:#ff4040;'>■</span> MAGNETIZED"
                    )
                    hud.adjustSize()
                prev = cur

        # ---- procesar cada plano ----
        for axis in planes:
            planes[axis]['events'] = [
                e for e in planes[axis]['events']
                if (now - e['t']) <= HISTORY_SECONDS
            ]

        for axis in ['x', 'y', 'z']:
            p = planes[axis]
            H_total    = np.zeros_like(XX, dtype=float)
            color_acc  = np.zeros((GRID_Y, GRID_X, 3), dtype=float)
            weight_acc = np.zeros_like(XX, dtype=float)

            for ev in p['events']:
                t_el = now - ev['t']
                xi   = X_START - V * t_el

                if t_el < FADE_IN_TIME:
                    tf = t_el / FADE_IN_TIME
                elif t_el > HISTORY_SECONDS - FADE_OUT_TIME:
                    tf = (HISTORY_SECONDS - t_el) / FADE_OUT_TIME
                else:
                    tf = 1.0
                tf = max(0.0, min(1.0, tf))

                u = (XX - xi) / W_HALF
                envelope = tukey_window(u)
                osc = np.cos(K_OSC * (XX - xi))

                amp = min(MAX_AMP_PER_EVENT, ev['delta'] * AMP_GAIN)
                H_i = amp * tf * envelope * osc

                w = np.abs(H_i) + 1e-6
                H_total            += H_i
                color_acc[:, :, 0] += w * ev['color'][0]
                color_acc[:, :, 1] += w * ev['color'][1]
                color_acc[:, :, 2] += w * ev['color'][2]
                weight_acc         += w

            if weight_acc.max() > 0 and CROSS_RIPPLE > 0:
                h_peak = float(np.max(np.abs(H_total)))
                if h_peak > 0.5:
                    H_total += CROSS_RIPPLE * h_peak * np.sin(
                        2 * np.pi * 3 * YY / POOL_D + now * 1.2
                    )

            mask = weight_acc > 1e-4
            color_final = np.zeros_like(color_acc)
            color_final[mask] = color_acc[mask] / weight_acc[mask, None]
            color_final[~mask] = p['tint']

            h_min, h_max = float(H_total.min()), float(H_total.max())
            hr = h_max - h_min
            if hr < 0.5:
                bright = np.full_like(H_total, 0.5)
            else:
                hn = (H_total - h_min) / hr
                bright = 0.20 + 0.80 * (hn ** 1.5)

            pulse = 0.72 + 0.28 * np.sin(now * MAG_PULSE_FREQ)
            red_mask = (
                (color_final[:, :, 0] > 0.6) &
                (color_final[:, :, 1] < 0.35) &
                (color_final[:, :, 2] < 0.35) &
                mask
            )
            bright = np.where(red_mask, bright * pulse, bright)

            colors = np.zeros((GRID_X * GRID_Y, 4), dtype=float)
            colors[:, 0] = (color_final[:, :, 0] * bright).ravel()
            colors[:, 1] = (color_final[:, :, 1] * bright).ravel()
            colors[:, 2] = (color_final[:, :, 2] * bright).ravel()
            colors[:, 3] = 0.90

            new_verts = p['verts'].copy()
            new_verts[:, 2] = H_total.ravel()
            p['item'].setMeshData(
                vertexes=new_verts,
                faces=faces,
                vertexColors=colors
            )

    except Exception as e:
        print("ERROR:", e)

# =====================================
# TIMER
# =====================================
timer = QTimer()
timer.timeout.connect(update)
timer.start(20)

main_window.show()
sys.exit(app.exec())
