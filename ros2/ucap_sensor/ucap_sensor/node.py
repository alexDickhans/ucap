"""ROS 2 node hosting uCap v1 USB CDC telemetry."""

from __future__ import annotations

import math

import rclpy
from rclpy.node import Node
from sensor_msgs.msg import MagneticField, Range
from std_msgs.msg import Bool, Float64, String

from ucap_sensor.serial_parser import parse_telemetry_line

try:
    import serial
    from serial import SerialException
except ImportError:  # pragma: no cover - runtime dependency
    serial = None
    SerialException = OSError


class UcapSensorNode(Node):
    """Read firmware CDC lines and publish idiomatic sensor topics."""

    def __init__(self) -> None:
        super().__init__('ucap_sensor')

        self.declare_parameter('port', '/dev/ttyACM0')
        self.declare_parameter('baud', 115200)
        self.declare_parameter('frame_id', 'ucap_link')
        self.declare_parameter('publish_raw', True)
        self.declare_parameter('tof_fov_rad', 0.785398)  # ~45 deg; VL53L5CX FOV approx
        self.declare_parameter('tof_min_range_m', 0.02)
        self.declare_parameter('tof_max_range_m', 4.0)
        self.declare_parameter('reconnect_period_s', 1.0)
        self.declare_parameter('read_period_s', 0.02)

        self._port = self.get_parameter('port').get_parameter_value().string_value
        self._baud = int(self.get_parameter('baud').get_parameter_value().integer_value)
        self._frame_id = self.get_parameter('frame_id').get_parameter_value().string_value
        self._publish_raw = self.get_parameter('publish_raw').get_parameter_value().bool_value
        self._tof_fov = float(self.get_parameter('tof_fov_rad').value)
        self._tof_min = float(self.get_parameter('tof_min_range_m').value)
        self._tof_max = float(self.get_parameter('tof_max_range_m').value)

        self._pub_cap = self.create_publisher(Float64, 'capacitance', 10)
        self._pub_cap_ff = self.create_publisher(Float64, 'capacitance_fF', 10)
        self._pub_cap_valid = self.create_publisher(Bool, 'capacitance_valid', 10)
        self._pub_t_us = self.create_publisher(Float64, 'charge_time_us', 10)
        self._pub_sense = self.create_publisher(Float64, 'adc/sense_voltage', 10)
        self._pub_vth = self.create_publisher(Float64, 'adc/vth_voltage', 10)
        self._pub_mag = self.create_publisher(MagneticField, 'magnetic_field', 10)
        self._pub_tof = [
            self.create_publisher(Range, f'tof{i}/range', 10) for i in range(3)
        ]
        self._pub_tof_present = [
            self.create_publisher(Bool, f'tof{i}/present', 10) for i in range(3)
        ]
        self._pub_raw = (
            self.create_publisher(String, 'raw', 10) if self._publish_raw else None
        )

        self._ser = None
        self._buf = ''

        reconnect_s = float(self.get_parameter('reconnect_period_s').value)
        read_s = float(self.get_parameter('read_period_s').value)
        self.create_timer(reconnect_s, self._ensure_serial)
        self.create_timer(read_s, self._poll_serial)

        self.get_logger().info(
            f'uCap sensor node starting (port={self._port}, baud={self._baud})'
        )
        self._ensure_serial()

    def destroy_node(self) -> bool:
        self._close_serial()
        return super().destroy_node()

    def _close_serial(self) -> None:
        if self._ser is not None:
            try:
                self._ser.close()
            except Exception:  # noqa: BLE001
                pass
            self._ser = None

    def _ensure_serial(self) -> None:
        if serial is None:
            self.get_logger().error('pyserial is not installed (python3-serial)')
            return
        if self._ser is not None and self._ser.is_open:
            return
        try:
            self._ser = serial.Serial(
                port=self._port,
                baudrate=self._baud,
                timeout=0,
            )
            self._buf = ''
            self.get_logger().info(f'Opened {self._port} @ {self._baud}')
        except SerialException as exc:
            self._ser = None
            self.get_logger().warn(f'Waiting for {self._port}: {exc}', throttle_duration_sec=5.0)

    def _poll_serial(self) -> None:
        if self._ser is None or not self._ser.is_open:
            return
        try:
            waiting = self._ser.in_waiting
            if waiting:
                chunk = self._ser.read(waiting)
                self._buf += chunk.decode('utf-8', errors='replace')
        except SerialException as exc:
            self.get_logger().warn(f'Serial read failed: {exc}')
            self._close_serial()
            return

        while True:
            nl = self._buf.find('\n')
            if nl < 0:
                break
            line = self._buf[:nl].rstrip('\r')
            self._buf = self._buf[nl + 1:]
            self._handle_line(line)

    def _handle_line(self, line: str) -> None:
        if not line:
            return

        if self._pub_raw is not None:
            msg = String()
            msg.data = line
            self._pub_raw.publish(msg)

        sample = parse_telemetry_line(line)
        if sample is None:
            self.get_logger().debug(f'boot/status: {line}')
            return

        stamp = self.get_clock().now().to_msg()

        valid = Bool()
        valid.data = sample.capacitance_valid
        self._pub_cap_valid.publish(valid)

        if sample.capacitance_valid and sample.capacitance_fF is not None:
            ff = Float64()
            ff.data = sample.capacitance_fF
            self._pub_cap_ff.publish(ff)
            farads = Float64()
            farads.data = sample.capacitance_fF * 1e-15
            self._pub_cap.publish(farads)

        if sample.charge_time_us is not None:
            t_us = Float64()
            t_us.data = sample.charge_time_us
            self._pub_t_us.publish(t_us)

        if sample.sense_v is not None:
            sense = Float64()
            sense.data = sample.sense_v
            self._pub_sense.publish(sense)

        if sample.vth_v is not None:
            vth = Float64()
            vth.data = sample.vth_v
            self._pub_vth.publish(vth)

        if sample.mag_valid and sample.mag_uT is not None:
            mag = MagneticField()
            mag.header.stamp = stamp
            mag.header.frame_id = self._frame_id
            # Firmware prints microtesla; sensor_msgs uses tesla.
            mag.magnetic_field.x = sample.mag_uT[0] * 1e-6
            mag.magnetic_field.y = sample.mag_uT[1] * 1e-6
            mag.magnetic_field.z = sample.mag_uT[2] * 1e-6
            # -1 in [0] means covariance unknown (sensor_msgs convention).
            mag.magnetic_field_covariance = [-1.0] + [0.0] * 8
            self._pub_mag.publish(mag)

        for i, reading in enumerate(sample.tof):
            present = Bool()
            present.data = reading.present
            self._pub_tof_present[i].publish(present)

            if not reading.present or reading.waiting or reading.distance_mm is None:
                continue

            rng = Range()
            rng.header.stamp = stamp
            rng.header.frame_id = f'{self._frame_id}_tof{i}'
            rng.radiation_type = Range.INFRARED
            rng.field_of_view = self._tof_fov
            rng.min_range = self._tof_min
            rng.max_range = self._tof_max
            # Firmware prints center-zone mm; sensor_msgs/Range uses meters.
            rng.range = float(reading.distance_mm) * 1e-3
            if math.isnan(rng.range):
                continue
            self._pub_tof[i].publish(rng)


def main(args=None) -> None:
    rclpy.init(args=args)
    node = UcapSensorNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
