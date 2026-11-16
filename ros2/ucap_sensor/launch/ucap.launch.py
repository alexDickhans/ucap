from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue


def generate_launch_description() -> LaunchDescription:
    port = LaunchConfiguration('port')
    baud = LaunchConfiguration('baud')
    frame_id = LaunchConfiguration('frame_id')
    publish_raw = LaunchConfiguration('publish_raw')

    return LaunchDescription([
        DeclareLaunchArgument('port', default_value='/dev/ttyACM0'),
        DeclareLaunchArgument('baud', default_value='115200'),
        DeclareLaunchArgument('frame_id', default_value='ucap_link'),
        DeclareLaunchArgument('publish_raw', default_value='true'),
        Node(
            package='ucap_sensor',
            executable='ucap_sensor_node',
            name='ucap_sensor',
            namespace='ucap',
            output='screen',
            parameters=[{
                'port': port,
                'baud': ParameterValue(baud, value_type=int),
                'frame_id': frame_id,
                'publish_raw': ParameterValue(publish_raw, value_type=bool),
            }],
        ),
    ])
