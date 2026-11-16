"""Parse uCap v1 USB CDC telemetry lines exactly as firmware emits them.

Source of truth: firmware/src/main.cpp (loop print every ~200 ms).
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from typing import List, Optional


# Boot / status lines (ignored by parse_telemetry_line):
#   "uCap v1"
#   "pins: PA0 charge, PA1 COMP1_INP, PA4 COMP1_INM, PA5 LED"
#   "LIS3MDL ok on I2C0 (J3)" | "LIS3MDL missing on I2C0"
#   "VL53L5CX present: <N>"
#
# Telemetry line template (spaces between fields, trailing newline stripped):
#   C_fF=<float|timeout> [t_us=<float>] sense_V=<float> vth_V=<float>
#     [mag_uT=<x>,<y>,<z>] tof0=<-|wait|<int>mm> tof1=... tof2=...


@dataclass
class TofReading:
    """One VL53L5CX center-zone sample from the CDC line."""

    present: bool = False
    waiting: bool = False
    distance_mm: Optional[int] = None


@dataclass
class TelemetrySample:
    """One parsed telemetry line from the firmware CDC stream."""

    raw: str
    capacitance_valid: bool = False
    capacitance_fF: Optional[float] = None
    charge_time_us: Optional[float] = None
    sense_v: Optional[float] = None
    vth_v: Optional[float] = None
    mag_valid: bool = False
    mag_uT: Optional[tuple] = None  # (x, y, z)
    tof: List[TofReading] = field(default_factory=lambda: [TofReading() for _ in range(3)])


_RE_C_FF = re.compile(r'^C_fF=(timeout|[+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?)')
_RE_T_US = re.compile(r'\bt_us=([+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?)')
_RE_SENSE = re.compile(r'\bsense_V=([+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?)')
_RE_VTH = re.compile(r'\bvth_V=([+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?)')
_RE_MAG = re.compile(
    r'\bmag_uT=([+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?),'
    r'([+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?),'
    r'([+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?)'
)
# Order matters: bare "-" is "absent"; "<int>mm" is a range (must win over "-").
_RE_TOF = re.compile(r'\btof([0-2])=(wait|(-?\d+)mm|-)')


def is_telemetry_line(line: str) -> bool:
    """True if the line starts with the capacitance field prefix."""
    return line.startswith('C_fF=')


def parse_telemetry_line(line: str) -> Optional[TelemetrySample]:
    """Parse one firmware telemetry line. Returns None for non-telemetry text."""
    text = line.strip()
    if not is_telemetry_line(text):
        return None

    m_c = _RE_C_FF.match(text)
    if not m_c:
        return None

    sample = TelemetrySample(raw=text)
    c_token = m_c.group(1)
    if c_token != 'timeout':
        sample.capacitance_valid = True
        sample.capacitance_fF = float(c_token)
        m_t = _RE_T_US.search(text)
        if m_t:
            sample.charge_time_us = float(m_t.group(1))

    m_s = _RE_SENSE.search(text)
    if m_s:
        sample.sense_v = float(m_s.group(1))

    m_v = _RE_VTH.search(text)
    if m_v:
        sample.vth_v = float(m_v.group(1))

    m_mag = _RE_MAG.search(text)
    if m_mag:
        sample.mag_valid = True
        sample.mag_uT = (float(m_mag.group(1)), float(m_mag.group(2)), float(m_mag.group(3)))

    for m_tof in _RE_TOF.finditer(text):
        idx = int(m_tof.group(1))
        token = m_tof.group(2)
        reading = TofReading()
        if token == 'wait':
            reading.present = True
            reading.waiting = True
        elif token == '-':
            reading.present = False
        else:
            # token is like "320mm"; capture group 3 is the integer.
            reading.present = True
            reading.waiting = False
            reading.distance_mm = int(m_tof.group(3))
        sample.tof[idx] = reading

    return sample
