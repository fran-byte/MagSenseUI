import sys
import re
import time
import serial
import serial.tools.list_ports
import numpy as np
from PyQt6.QtWidgets import (
    QApplication, QWidget, QHBoxLayout, QVBoxLayout, QLabel, QSizePolicy,
    QDialog, QComboBox, QPushButton, QMessageBox, QFormLayout, QStackedWidget
)
from PyQt6.QtCore import QTimer, Qt
import pyqtgraph.opengl as gl
import pyqtgraph as pg

# =====================================
# PRESETS
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
else:
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
TINT_EMPTY = np.array([0.06, 0.07, 0.11])

BG_COLOR = pg.mkColor(4, 10, 26, 255)

# =====================================
# CILINDROS (vúmetro circular, avance angular continuo tipo reloj)
# =====================================
CYL_SEGMENTS   = 72          # resolución angular del arco
CYL_RADIUS     = 100.0
CYL_HEIGHT     = 24.0        # cilindro bajo, tipo disco
CYL_EMPTY_ALPHA = 0.35       # brillo del contorno vacío
CYL_MAX_DELTA  = 140.0       # delta que llena el cilindro al 100 %
CYL_RISE       = 0.25        # suavizado al subir (0..1 por frame)
CYL_FALL       = 0.10        # suavizado al bajar
CYL_STOPS      = [0.0, 0.55, 0.80, 1.0]   # verde -> amarillo -> rojo según el ángulo barrido

# =====================================
# =====================================
# DIÁLOGO INICIAL
# =====================================
def ask_system_and_port():
    dlg = QDialog()
    dlg.setWindowTitle("MagSenseUI — Configuración")
    dlg.setModal(True)
    dlg.setMinimumWidth(460)
    layout = QVBoxLayout(dlg)
    layout.setContentsMargins(18, 18, 18, 18)
    layout.setSpacing(10)
    layout.addWidget(QLabel("<b>Selecciona el sistema y el puerto serie</b>"))
    form = QFormLayout()
    form.setSpacing(8)

    cb_os = QComboBox()
    cb_os.addItems(["Linux", "Windows"])
    cb_os.setCurrentText("Windows" if sys.platform.startswith("win") else "Linux")
    form.addRow("Sistema:", cb_os)

    cb_port = QComboBox()
    cb_port.setEditable(True)
    form.addRow("Puerto:", cb_port)

    def refresh_ports():
        cb_port.clear()
        ports = list(serial.tools.list_ports.comports())
        usb_like, tty_s = [], []
        for p in ports:
            if "ttyACM" in p.device or "ttyUSB" in p.device: usb_like.append(p)
            elif "ttyS" in p.device: tty_s.append(p)
            else: usb_like.append(p)
        candidates = usb_like if usb_like else tty_s
        if candidates:
            for p in candidates:
                label = p.device
                if p.description and p.description != "n/a":
                    label = f"{p.device}  —  {p.description}"
                cb_port.addItem(label, userData=p.device)
            cb_port.setCurrentText(candidates[0].device)
        else:
            os_now = cb_os.currentText()
            defaults = ["/dev/ttyACM0", "/dev/ttyUSB0"] if os_now == "Linux" else ["COM5", "COM3"]
            for it in defaults: cb_port.addItem(it)
            cb_port.setCurrentText(defaults[0])

    cb_os.currentTextChanged.connect(lambda _: refresh_ports())
    refresh_ports()

    btn_scan = QPushButton("🔄 Re-escanear puertos")
    btn_scan.clicked.connect(refresh_ports)
    form.addRow("", btn_scan)
    layout.addLayout(form)

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
        idx = cb_port.currentIndex()
        port = cb_port.itemData(idx) if idx >= 0 and cb_port.itemData(idx) else cb_port.currentText().strip().split("—")[0].strip()
        if not port:
            QMessageBox.warning(dlg, "Puerto vacío", "Introduce un puerto.")
            return
        try:
            s = serial.Serial(port, BAUDRATE, timeout=0.02)
            result["ser"] = s
            result["system"] = cb_os.currentText()
            result["port"] = port
            dlg.accept()
        except Exception as e:
            QMessageBox.critical(dlg, "No se pudo abrir", f"Puerto: {port}\n\n{e}")

    btn_ok.clicked.connect(try_connect)
    btn_cancel.clicked.connect(dlg.reject)
    if dlg.exec() != QDialog.DialogCode.Accepted:
        return None, None, None
    return result["ser"], result["system"], result["port"]

# =====================================
# APP
# =====================================
app = QApplication(sys.argv)
ser, SYSTEM_LABEL, PORT_NAME = ask_system_and_port()
if ser is None: sys.exit(0)
print(f"Puerto: {PORT_NAME}  ({SYSTEM_LABEL})")

# =====================================
# VENTANA
# =====================================
main_window = QWidget()
main_window.setWindowTitle(f"MagSenseUI 3D  [{PRESET}]  [{SYSTEM_LABEL} · {PORT_NAME}]")
main_window.resize(1200, 1400)

root_layout = QVBoxLayout(main_window)
root_layout.setContentsMargins(6, 6, 6, 6)
root_layout.setSpacing(4)

# --- Barra superior ---
mode_bar = QHBoxLayout()
mode_bar.setSpacing(6)
btn_mode_waves = QPushButton("🌊 Olas")
btn_mode_waves.setCheckable(True)
btn_mode_waves.setChecked(True)
btn_mode_waves.setStyleSheet("""
    QPushButton { background:#183a5a; color:#cfe3ff; border:1px solid #2a6fff; border-radius:6px; padding:6px 16px; font-weight:bold; }
    QPushButton:checked { background:#2a6fff; color:white; }
""")
btn_mode_cheese = QPushButton("🛢️ Cilindros")
btn_mode_cheese.setCheckable(True)
btn_mode_cheese.setStyleSheet("""
    QPushButton { background:#183a5a; color:#cfe3ff; border:1px solid #2a6fff; border-radius:6px; padding:6px 16px; font-weight:bold; }
    QPushButton:checked { background:#2a6fff; color:white; }
""")
mode_bar.addWidget(btn_mode_waves)
mode_bar.addWidget(btn_mode_cheese)
mode_bar.addStretch(1)
root_layout.addLayout(mode_bar)

hud = QLabel("X: --  Y: --  Z: --")
hud.setTextFormat(Qt.TextFormat.RichText)
hud.setStyleSheet("""
    QLabel {
        color: #7fe7ff; background: rgba(0,20,40,220);
        border: 1px solid #2a6fff; border-radius: 8px; padding: 8px 12px;
        font-family: 'Consolas', monospace; font-size: 13px;
    }
""")
root_layout.addWidget(hud)

stack = QStackedWidget()
root_layout.addWidget(stack, stretch=1)

# =====================================
# MODO OLAS
# =====================================
waves_page = QWidget()
waves_layout = QVBoxLayout(waves_page)
waves_layout.setContentsMargins(0, 0, 0, 0); waves_layout.setSpacing(6)

def make_wave_view(title_text):
    container = QWidget()
    vbox = QVBoxLayout(container)
    vbox.setContentsMargins(0, 0, 0, 0); vbox.setSpacing(0)
    label = QLabel(title_text)
    label.setAlignment(Qt.AlignmentFlag.AlignCenter)
    label.setStyleSheet("""
        QLabel {
            color:#cfe3ff; background:rgba(0,10,30,200);
            border:1px solid #1e3a6a; border-top-left-radius:6px; border-top-right-radius:6px;
            padding:3px; font-family:'Consolas'; font-size:11px; font-weight:bold;
        }
    """)
    vbox.addWidget(label)
    view = gl.GLViewWidget()
    view.setBackgroundColor(BG_COLOR)
    view.setCameraPosition(distance=750, elevation=22, azimuth=18)
    view.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
    vbox.addWidget(view, stretch=1)
    return container, view

wave_container_x, wave_view_x = make_wave_view("EJE X")
wave_container_y, wave_view_y = make_wave_view("EJE Y")
wave_container_z, wave_view_z = make_wave_view("EJE Z")
waves_layout.addWidget(wave_container_x, stretch=1)
waves_layout.addWidget(wave_container_y, stretch=1)
waves_layout.addWidget(wave_container_z, stretch=1)
stack.addWidget(waves_page)

wave_views = {'x': wave_view_x, 'y': wave_view_y, 'z': wave_view_z}
wave_last_cam = {'x': None, 'y': None, 'z': None}

wxx = np.linspace(-POOL_W/2, POOL_W/2, GRID_X)
wyy = np.linspace(-POOL_D/2, POOL_D/2, GRID_Y)
WXX, WYY = np.meshgrid(wxx, wyy)

wave_faces = []
for j in range(GRID_Y - 1):
    for i in range(GRID_X - 1):
        a = j * GRID_X + i; b = a + 1; c = a + GRID_X; d = c + 1
        wave_faces.append([a, b, c]); wave_faces.append([b, d, c])
wave_faces = np.array(wave_faces, dtype=np.uint32)

def make_wave_plane(view, tint):
    verts = np.zeros((GRID_X * GRID_Y, 3), dtype=float)
    verts[:, 0] = WXX.ravel(); verts[:, 1] = WYY.ravel(); verts[:, 2] = 0.0
    colors = np.zeros((GRID_X * GRID_Y, 4), dtype=float)
    colors[:, 0] = tint[0]; colors[:, 1] = tint[1]; colors[:, 2] = tint[2]; colors[:, 3] = 0.55
    mesh = gl.MeshData(vertexes=verts, faces=wave_faces)
    item = gl.GLMeshItem(meshdata=mesh, smooth=True, shader='shaded',
                         glOptions='additive', drawEdges=False)
    view.addItem(item)
    return item, verts

wave_planes = {}
for ax, v, t in [('x', wave_view_x, TINT_X), ('y', wave_view_y, TINT_Y), ('z', wave_view_z, TINT_Z)]:
    item, verts = make_wave_plane(v, t)
    wave_planes[ax] = {'item': item, 'verts': verts, 'events': [], 'tint': t, 'view': v}

# =====================================
# MODO QUESITOS (vunetro circular)
# =====================================
cheese_page = QWidget()
cheese_layout = QVBoxLayout(cheese_page)
cheese_layout.setContentsMargins(0, 0, 0, 0); cheese_layout.setSpacing(6)

def make_cheese_view(title_text):
    container = QWidget()
    vbox = QVBoxLayout(container)
    vbox.setContentsMargins(0, 0, 0, 0); vbox.setSpacing(0)
    label = QLabel(title_text)
    label.setAlignment(Qt.AlignmentFlag.AlignCenter)
    label.setStyleSheet("""
        QLabel {
            color:#cfe3ff; background:rgba(0,10,30,200);
            border:1px solid #1e3a6a; border-top-left-radius:6px; border-top-right-radius:6px;
            padding:3px; font-family:'Consolas'; font-size:11px; font-weight:bold;
        }
    """)
    vbox.addWidget(label)
    view = gl.GLViewWidget()
    view.setBackgroundColor(BG_COLOR)
    view.setCameraPosition(pos=pg.Vector(0, 0, CYL_HEIGHT / 2), distance=380, elevation=55, azimuth=30)
    view.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
    vbox.addWidget(view, stretch=1)
    return container, view

cheese_container_x, cheese_view_x = make_cheese_view("EJE X")
cheese_container_y, cheese_view_y = make_cheese_view("EJE Y")
cheese_container_z, cheese_view_z = make_cheese_view("EJE Z")
cheese_layout.addWidget(cheese_container_x, stretch=1)
cheese_layout.addWidget(cheese_container_y, stretch=1)
cheese_layout.addWidget(cheese_container_z, stretch=1)
stack.addWidget(cheese_page)

cheese_views = {'x': cheese_view_x, 'y': cheese_view_y, 'z': cheese_view_z}
cheese_last_cam = {'x': None, 'y': None, 'z': None}

# --- Contorno (cilindro vacío, tenue) ---
def build_cyl_shell():
    S = CYL_SEGMENTS
    ang = np.linspace(0, 2*np.pi, S, endpoint=False)
    verts = np.zeros((2*S + 2, 3), dtype=float)
    for i in range(S):
        verts[i]     = [CYL_RADIUS*np.cos(ang[i]), CYL_RADIUS*np.sin(ang[i]), 0.0]
        verts[S + i] = [CYL_RADIUS*np.cos(ang[i]), CYL_RADIUS*np.sin(ang[i]), CYL_HEIGHT]
    verts[2*S]     = [0.0, 0.0, 0.0]
    verts[2*S + 1] = [0.0, 0.0, CYL_HEIGHT]
    faces = []
    for i in range(S):
        i2 = (i + 1) % S
        faces.append([i, i2, S + i]); faces.append([i2, S + i2, S + i])
        faces.append([2*S, i2, i])
        faces.append([2*S + 1, S + i, S + i2])
    return verts, np.array(faces, dtype=np.uint32)

# --- Relleno: cuña (porción de tarta) cuyo ángulo crece de forma continua.
# Topología fija; solo cambian posiciones y colores.
# Índices: tapa [0 = centro, 1..S+1 = arco] · lateral [S+2.. base, 2S+3.. techo] · 2 paredes radiales [3S+4..]
def build_cyl_fill():
    S = CYL_SEGMENTS
    sb, st, W = S + 2, 2*S + 3, 3*S + 4
    faces = []
    for i in range(S):
        faces.append([0, 1 + i, 2 + i])
        a, b, c, d = sb + i, sb + i + 1, st + i, st + i + 1
        faces.append([a, b, c]); faces.append([b, d, c])
    for w in (W, W + 4):
        faces.append([w, w + 2, w + 1]); faces.append([w + 2, w + 3, w + 1])
    verts = np.zeros((3*S + 12, 3), dtype=float)
    return verts, np.array(faces, dtype=np.uint32)

def cyl_gradient(f):
    """f (0..1 = fracción de altura total) -> RGB verde/amarillo/rojo."""
    cols = np.array([COLOR_OK, COLOR_OK, COLOR_WARNING, COLOR_MAGNETIZED])
    return np.stack([np.interp(f, CYL_STOPS, cols[:, c]) for c in range(3)], axis=-1)

cheese_discs = {}
for ax, v, t in [('x', cheese_view_x, TINT_X), ('y', cheese_view_y, TINT_Y), ('z', cheese_view_z, TINT_Z)]:
    # contorno
    sh_verts, sh_faces = build_cyl_shell()
    sh_colors = np.zeros((len(sh_verts), 4), dtype=float)
    sh_colors[:, 0] = t[0] * 2.0; sh_colors[:, 1] = t[1] * 2.0; sh_colors[:, 2] = t[2] * 2.0
    sh_colors[:, 3] = CYL_EMPTY_ALPHA
    shell = gl.GLMeshItem(vertexes=sh_verts, faces=sh_faces, vertexColors=sh_colors,
                          smooth=False, shader=None, glOptions='additive', drawEdges=False)
    v.addItem(shell)

    # relleno
    verts, faces = build_cyl_fill()
    colors = np.ones((len(verts), 4), dtype=float)
    item = gl.GLMeshItem(vertexes=verts, faces=faces, vertexColors=colors,
                         smooth=False, shader=None, glOptions='translucent', drawEdges=False)
    item.setVisible(False)
    v.addItem(item)

    g = gl.GLGridItem()
    g.scale(30, 30, 1)
    g.translate(0, 0, -5)
    g.setColor((40, 70, 140, 55))
    v.addItem(g)

    cheese_discs[ax] = {
        'item': item,
        'view': v,
        'base_verts': verts.copy(),
        'faces': faces,
        'tint': t,
        'level': 0.0,     # nivel mostrado (animado)
        'target': 0.0,    # nivel objetivo
    }

# =====================================
# ESTADO COMÚN
# =====================================
prev = None
last_t = time.time()
current_mode = "waves"

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
    return np.where(absu <= 1 - alpha, 1.0, np.where(absu < 1, smooth, 0.0))

def add_cheese_event(axis, delta):
    """Fija el nivel objetivo = delta / CYL_MAX_DELTA (saturado a 1.0). El cilindro sube suavemente hasta él."""
    cheese_discs[axis]['target'] = float(np.clip(delta / CYL_MAX_DELTA, 0.0, 1.0))

def _cyl_color(f, pulse):
    """Color por fracción de vuelta f (0..1); la zona roja pulsa."""
    f = np.atleast_1d(np.asarray(f, dtype=float))
    rgb = cyl_gradient(f)
    fac = 1.0 - (1.0 - pulse) * np.clip((f - 0.85) / 0.15, 0.0, 1.0)
    return rgb * fac[:, None]

def update_cheese(now):
    S = CYL_SEGMENTS
    H, Rad = CYL_HEIGHT, CYL_RADIUS
    sb, st, W = S + 2, 2*S + 3, 3*S + 4
    idx = np.arange(S + 1)
    pulse = 0.72 + 0.28 * np.sin(now * MAG_PULSE_FREQ)
    for axis, disc in cheese_discs.items():
        # avance progresivo hacia el objetivo
        k = CYL_RISE if disc['target'] > disc['level'] else CYL_FALL
        disc['level'] += (disc['target'] - disc['level']) * k
        if abs(disc['target'] - disc['level']) < 1e-3:
            disc['level'] = disc['target']
        level = disc['level']

        if level < 0.004:
            disc['item'].setVisible(False)
            continue
        disc['item'].setVisible(True)

        ang = level * 2*np.pi * idx / S          # ángulo de cada vértice del arco
        f   = level * idx / S                    # fracción de vuelta -> color
        cx, cy = Rad * np.cos(ang), Rad * np.sin(ang)

        verts = np.zeros((3*S + 12, 3), dtype=float)
        verts[0] = [0.0, 0.0, H]
        verts[1:S+2] = np.column_stack([cx, cy, np.full(S + 1, H)])
        verts[sb:sb+S+1] = np.column_stack([cx, cy, np.zeros(S + 1)])
        verts[st:st+S+1] = np.column_stack([cx, cy, np.full(S + 1, H)])
        verts[W:W+4]   = [[0, 0, 0], [0, 0, H], [Rad, 0, 0], [Rad, 0, H]]
        verts[W+4:W+8] = [[0, 0, 0], [0, 0, H], [cx[-1], cy[-1], 0], [cx[-1], cy[-1], H]]

        rgb = _cyl_color(f, pulse)               # (S+1, 3)
        side = 0.80 + 0.20 * np.cos(ang - 0.6)   # sombreado suave del lateral
        colors = np.ones((3*S + 12, 4), dtype=float)
        colors[0, :3] = _cyl_color(level * 0.5, pulse)[0]
        colors[1:S+2, :3] = rgb                  # tapa a color pleno
        colors[sb:sb+S+1, :3] = rgb * side[:, None]
        colors[st:st+S+1, :3] = rgb * side[:, None]
        colors[W:W+4, :3]   = _cyl_color(0.0, pulse)[0] * 0.65
        colors[W+4:W+8, :3] = _cyl_color(level, pulse)[0] * 0.65
        colors[:, :3] = np.clip(colors[:, :3], 0.0, 1.0)

        disc['item'].setMeshData(vertexes=verts, faces=disc['faces'], vertexColors=colors)

# =====================================
# CAMBIO DE MODO
# =====================================
def switch_to_waves():
    global current_mode
    current_mode = "waves"
    stack.setCurrentIndex(0)
    btn_mode_waves.setChecked(True)
    btn_mode_cheese.setChecked(False)

def switch_to_cheese():
    global current_mode
    current_mode = "cheese"
    stack.setCurrentIndex(1)
    btn_mode_waves.setChecked(False)
    btn_mode_cheese.setChecked(True)

btn_mode_waves.clicked.connect(switch_to_waves)
btn_mode_cheese.clicked.connect(switch_to_cheese)

# =====================================
# UPDATE
# =====================================
def update():
    global prev, last_t
    try:
        now = time.time()
        last_t = now

        if current_mode == "waves":
            for name, v in wave_views.items():
                sig = (float(v.opts['distance']), float(v.opts['elevation']),
                       float(v.opts['azimuth']), tuple(v.opts['center']))
                if wave_last_cam[name] is None:
                    wave_last_cam[name] = sig
                elif sig != wave_last_cam[name]:
                    wave_last_cam[name] = sig
                    for on, ov in wave_views.items():
                        if ov is v: continue
                        ov.setCameraPosition(
                            pos=v.opts['center'], distance=v.opts['distance'],
                            elevation=v.opts['elevation'], azimuth=v.opts['azimuth'],
                        )
                        wave_last_cam[on] = sig
                    break
        elif current_mode == "cheese":
            for name, v in cheese_views.items():
                sig = (float(v.opts['distance']), float(v.opts['elevation']),
                       float(v.opts['azimuth']), tuple(v.opts['center']))
                if cheese_last_cam[name] is None:
                    cheese_last_cam[name] = sig
                elif sig != cheese_last_cam[name]:
                    cheese_last_cam[name] = sig
                    for on, ov in cheese_views.items():
                        if ov is v: continue
                        ov.setCameraPosition(
                            pos=v.opts['center'], distance=v.opts['distance'],
                            elevation=v.opts['elevation'], azimuth=v.opts['azimuth'],
                        )
                        cheese_last_cam[on] = sig
                    break

        line = ser.readline().decode(errors="ignore").strip()
        if line:
            m = re.search(
                r'X:\s([-\d.]+)\suT\sY:\s([-\d.]+)\suT\sZ:\s([-\d.]+)\suT', line
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
                            col = color_for_delta(d_axis)
                            wave_planes[axis]['events'].append({
                                't': now, 'delta': d_axis, 'color': col.copy()
                            })
                            add_cheese_event(axis, d_axis)

                    status, sc = status_info(delta_max)
                    counts = "  ".join(
                        f"<span style='color:#3fdd66;'>{a.upper()}:{len(wave_planes[a]['events'])}</span>"
                        for a in ['x', 'y', 'z']
                    )
                    cheese_counts = "  ".join(
                        f"{a.upper()}:{int(round(cheese_discs[a]['target'] * 100))}%"
                        for a in ['x', 'y', 'z']
                    )
                    hud.setText(
                        f"X: {x:7.2f}   Y: {y:7.2f}   Z: {z:7.2f} µT   │   "
                        f"Δx={d_abs[0]:5.1f}  Δy={d_abs[1]:5.1f}  Δz={d_abs[2]:5.1f}   │   "
                        f"<span style='color:{sc};'><b>{status}</b></span><br>"
                        f"olas: {counts}   │   cilindros: {cheese_counts}<br>"
                        f"<span style='color:#3fdd66;'>■</span> OK  "
                        f"<span style='color:#ffe012;'>■</span> WARNING  "
                        f"<span style='color:#ff4040;'>■</span> MAGNETIZED"
                    )
                    hud.adjustSize()
                prev = cur

        if current_mode == "waves":
            for axis in wave_planes:
                wave_planes[axis]['events'] = [
                    e for e in wave_planes[axis]['events']
                    if (now - e['t']) <= HISTORY_SECONDS
                ]
            for axis in ['x', 'y', 'z']:
                p = wave_planes[axis]
                H_total    = np.zeros_like(WXX, dtype=float)
                color_acc  = np.zeros((GRID_Y, GRID_X, 3), dtype=float)
                weight_acc = np.zeros_like(WXX, dtype=float)

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
                    u = (WXX - xi) / W_HALF
                    envelope = tukey_window(u)
                    osc = np.cos(K_OSC * (WXX - xi))
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
                            2 * np.pi * 3 * WYY / POOL_D + now * 1.2)

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
                p['item'].setMeshData(vertexes=new_verts, faces=wave_faces,
                                      vertexColors=colors)

        elif current_mode == "cheese":
            update_cheese(now)

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