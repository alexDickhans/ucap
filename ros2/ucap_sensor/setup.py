from setuptools import setup

package_name = 'ucap_sensor'

setup(
    name=package_name,
    version='0.1.0',
    packages=[package_name],
    data_files=[
        ('share/ament_index/resource_index/packages', ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        ('share/' + package_name + '/launch', ['launch/ucap.launch.py']),
        ('share/' + package_name + '/config', ['config/ucap_sensor.yaml']),
    ],
    install_requires=['setuptools', 'pyserial'],
    zip_safe=True,
    maintainer='Alex Dickhans',
    maintainer_email='alexdickhans@users.noreply.github.com',
    description='ROS 2 host for uCap v1 USB CDC sensor telemetry',
    license='MIT',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'ucap_sensor_node = ucap_sensor.node:main',
        ],
    },
)
