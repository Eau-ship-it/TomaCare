# TomaCare

Hello, my name is Esteban, and I've been developing my personal project, TomaCare, with the help of HackClub. My goal is to create a revolutionary tool for the Colombian Andean region, specifically for the Central Western Metropolitan Area (AMCO) and its surrounding rural and mountainous areas, to cultivate tomatoes with greater control and ease.

TomaCare is an intelligent agent based on models with an IoT system designed to improve climate control in Chonto tomato crops in the region. It will maintain constant soil temperature and humidity, as well as automated light levels, CO2 levels, and irrigation times. It will also provide alerts for the pH of the fertigation water and nutrient levels in the soil. TomaCare aims to improve greenhouse autonomy and eliminate human error, as well as address the challenges of immediate action due to the region's characteristically changeable climate.

TomaCare
"A new way to grow"

Built specifically for medium-scale Colombian farmers in the Risaralda region.

Components and Technologies

Sensors

DHT22
Capacitive sensor (SEN0193)
BH1750
MQ135
SEN0161 + probe
RS485 multiparameter sensor

Actuators

2x 12V DC submersible water pump — MOSFET IRF540N
Fan and extractor 24V DC — 2-channel relay + XL6009 converter
Heater (12V PTC resistor) — MOSFET IRF540N
5V ultrasonic humidifier — 1-channel relay
Grow LED strips — MOSFET IRFZ44N with PWM
Servo-motor shade/blind system
Active buzzer and warning LEDs

Processing

ESP32

Software / Communication

Wi-Fi
MQTT
Modbus over RS485
Arduino IDE / C++
Firebase
HTML5

Model-Based Intelligent Agent (ABM)

Expert / knowledge-based system
Multilayer perceptron neural network
Memory module
Project Status

The project is still in development, but due to its hardware limitations(for now), it is impossible to release a real demo.
All sensors and actuator working together, the next step is to adapt it to a Firebase Database.

Author

Esteban Castaño Castaño
