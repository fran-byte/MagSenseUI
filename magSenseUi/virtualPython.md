# Mini tutorial: Virtualizar el proyecto MagSenseUI

## 📁 Paso 0: Ir al proyecto

```bash
cd ~/Proyectos/MagSenseUI/magSenseUi
```

> Ajusta la ruta si tu proyecto está en otro sitio.

---

## 🧰 Paso 1: Instalar dependencias del sistema (una sola vez)

En Linux Mint / Ubuntu / Debian:

```bash
sudo apt update
sudo apt install -y python3 python3-venv python3-pip \
    libgl1 libglu1-mesa libegl1 libxkbcommon-x11-0 \
    libxcb-cursor0 libxcb-xinerama0 libxcb-icccm4 \
    libxcb-image0 libxcb-keysyms1 libxcb-randr0 \
    libxcb-render-util0 libxcb-shape0 libxcb-xfixes0
```

> Si ya lo tienes, no pasa nada, sale "already installed".

Y añade tu usuario al grupo `dialout` para acceder al puerto serie:

```bash
sudo usermod -a -G dialout $USER
```

⚠️ **Cierra sesión y vuelve a entrar** para que el grupo surta efecto (o usa `newgrp dialout` en la terminal actual).

---

## 🐍 Paso 2: Crear el entorno virtual (venv)

```bash
cd ~/Proyectos/MagSenseUI/magSenseUi

# Borra el venv si existe uno roto o viejo
rm -rf venv

# Crea el venv nuevo
python3 -m venv venv

# Actívalo
source venv/bin/activate
```

Sabrás que está activo porque el prompt cambia:

```
(venv) p4c0@mintx:~/Proyectos/MagSenseUI/magSenseUi$
```

Comprueba que `python` apunta al venv:

```bash
which python
# Debe decir: /home/p4c0/Proyectos/MagSenseUI/magSenseUi/venv/bin/python
```

---

## 📦 Paso 3: Instalar las dependencias del proyecto

Con el venv **activado**:

```bash
pip install --upgrade pip
pip install PyQt6 pyqtgraph pyserial numpy PyOpenGL PyOpenGL_accelerate
```

Verifica que todo importa sin errores:

```bash
python -c "import PyQt6, pyqtgraph, serial, numpy, OpenGL; print('OK')"
```

Si dice `OK` → todo listo.

Si `PyOpenGL_accelerate` falla al compilar (raro), instala solo `PyOpenGL`:

```bash
pip install PyOpenGL
```

---

## 🚀 Paso 4: Ejecutar el programa

Con el venv activado:

```bash
python xyz.py
```

O **sin activar** (más cómodo para repetir):

```bash
./venv/bin/python xyz.py
```

Se abrirá el **diálogo de arranque** donde eliges:

- **Sistema**: Linux (seleccionado por defecto)
- **Puerto**: `/dev/ttyACM0` (o el que corresponda a tu ESP32)

Pulsa **Conectar**. Si todo va bien → se abre la ventana con las 3 vistas 3D.

---

## 🔌 Paso 5: Encontrar el puerto del ESP32

Si no sabes qué puerto usar, antes de arrancar el programa:

```bash
ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null
```

- Si dice `/dev/ttyACM0` → ese es tu ESP32 (USB nativo CDC)
- Si dice `/dev/ttyUSB0` → tu ESP32 usa chip CH340/CP2102 (USB-serie)

Si no aparece nada al conectar:

```bash
dmesg | tail -20
```

Justo después de enchufar el USB. Deberías ver algo tipo `cdc_acm ... ttyACM0`.  
Si no aparece → **cable USB de carga** (no datos) o driver faltante.

**Prueba rápida de que hay datos**:

```bash
stty -F /dev/ttyACM0 115200 raw -echo
cat /dev/ttyACM0
# Ctrl+C para salir
```

Deberías ver líneas con `X: ... uT Y: ... uT Z: ... uT`.

---

## 💾 Guardar dependencias para el futuro (opcional pero recomendado)

Con el venv activado:

```bash
pip freeze > requirements.txt
```

Y añade `requirements.txt` al repo:

```bash
git add requirements.txt
git commit -m "Añadir requirements.txt"
git push
```

Cuando clones el proyecto en otra máquina:

```bash
git clone <repo>
cd MagSenseUI/magSenseUi
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
python xyz.py
```

---

## ⚡ Atajo: alias para lanzar con un solo comando

Añade al final de `~/.bashrc`:

```bash
alias magsense='cd ~/Proyectos/MagSenseUI/magSenseUi && ./venv/bin/python xyz.py'
```

Recarga:

```bash
source ~/.bashrc
```

Y desde cualquier terminal:

```bash
magsense
```

---

## 🛠️ Solución de problemas rápidos

| Problema                                              | Solución                                                                                                 |
| ----------------------------------------------------- | -------------------------------------------------------------------------------------------------------- |
| `bash: venv/bin/python: No such file`                 | El venv está roto → `rm -rf venv && python3 -m venv venv && source venv/bin/activate && pip install ...` |
| `ModuleNotFoundError: No module named 'PyQt6'`        | No estás dentro del venv → `source venv/bin/activate`                                                    |
| `ModuleNotFoundError: No module named 'OpenGL'`       | `pip install PyOpenGL PyOpenGL_accelerate`                                                               |
| `Could not load the Qt platform plugin "xcb"`         | Faltan libs del sistema → `sudo apt install libxcb-cursor0 libxkbcommon-x11-0 ...` (Paso 1)              |
| `Permission denied: '/dev/ttyACM0'`                   | `sudo usermod -a -G dialout $USER` + cerrar sesión                                                       |
| `Could not configure port: (5, 'Input/output error')` | Puerto incorrecto → usa `/dev/ttyACM0` o `/dev/ttyUSB0`, no `/dev/ttyS*`                                 |
| `No module named 'serial'`                            | `pip install pyserial` dentro del venv                                                                   |

---

## 📋 Chuleta de comandos

```bash
# Entrar al proyecto
cd ~/Proyectos/MagSenseUI/magSenseUi

# Activar venv (cada terminal nueva)
source venv/bin/activate

# Comprobar que estás dentro
which python          # → .../venv/bin/python

# Ejecutar
python xyz.py

# Salir del venv
deactivate

# Sin activar, directo
./venv/bin/python xyz.py

# Ver puertos serie
ls /dev/ttyACM* /dev/ttyUSB*

# Probar si llegan datos
stty -F /dev/ttyACM0 115200 raw -echo && cat /dev/ttyACM0
```

---

## ✅ Verificación final (todo en un bloque)

```bash
cd ~/Proyectos/MagSenseUI/magSenseUi
rm -rf venv
python3 -m venv venv
source venv/bin/activate
pip install --upgrade pip
pip install PyQt6 pyqtgraph pyserial numpy PyOpenGL PyOpenGL_accelerate
python -c "import PyQt6, pyqtgraph, serial, numpy, OpenGL; print('✅ Todo OK')"
ls /dev/ttyACM* /dev/ttyUSB*
python xyz.py
```

Si el último `python -c ...` dice `✅ Todo OK` y el `ls` muestra tu puerto → **el programa arrancará sin problemas**.

---

Con esto tienes el proyecto virtualizado y ejecutándose. La próxima vez solo necesitas **3 comandos**:

```bash
cd ~/Proyectos/MagSenseUI/magSenseUi
source venv/bin/activate
python xyz.py
```
