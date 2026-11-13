"""Unit tests for firmware CDC line parsing (no ROS runtime required)."""

from ucap_sensor.serial_parser import is_telemetry_line, parse_telemetry_line


def test_boot_lines_ignored():
    assert parse_telemetry_line('uCap v1') is None
    assert parse_telemetry_line('LIS3MDL ok on I2C0 (J3)') is None
    assert parse_telemetry_line('VL53L5CX present: 2') is None
    assert not is_telemetry_line('uCap v1')


def test_full_line_with_mag_and_tof():
    line = (
        'C_fF=1234.5 t_us=0.86 sense_V=1.650 vth_V=1.648 '
        'mag_uT=12.3,-4.5,50.1 tof0=320mm tof1=- tof2=wait'
    )
    s = parse_telemetry_line(line)
    assert s is not None
    assert s.capacitance_valid
    assert s.capacitance_fF == 1234.5
    assert s.charge_time_us == 0.86
    assert s.sense_v == 1.650
    assert s.vth_v == 1.648
    assert s.mag_valid
    assert s.mag_uT == (12.3, -4.5, 50.1)
    assert s.tof[0].present and s.tof[0].distance_mm == 320
    assert not s.tof[1].present
    assert s.tof[2].present and s.tof[2].waiting


def test_timeout_without_mag():
    line = 'C_fF=timeout sense_V=0.012 vth_V=1.650 tof0=- tof1=- tof2=-'
    s = parse_telemetry_line(line)
    assert s is not None
    assert not s.capacitance_valid
    assert s.capacitance_fF is None
    assert s.charge_time_us is None
    assert s.sense_v == 0.012
    assert not s.mag_valid
    assert s.mag_uT is None
    assert all(not t.present for t in s.tof)


def test_negative_tof_distance_token():
    # Firmware uses int print; negative would be odd but format allows digits after optional sign.
    line = (
        'C_fF=10.0 t_us=0.01 sense_V=1.000 vth_V=1.000 '
        'tof0=-1mm tof1=0mm tof2=4095mm'
    )
    s = parse_telemetry_line(line)
    assert s is not None
    assert s.tof[0].distance_mm == -1
    assert s.tof[1].distance_mm == 0
    assert s.tof[2].distance_mm == 4095
