# TomaCare
Hola, mi nombre es Esteban y he estado desarrollando mi proyecto personal TomaCare con la ayuda de HackClub. Mi objetivo es podder generar una herramienta revolucionaria para el campo andino colombiano, especificamente para la region de el Area metropolitana de centro occidente (AMCO) y sus alrededores rurales y montañosos para cultivar tomate con mayor control y facilidad.

TomaCare se trata de un agente inteligente basado en modelos con un sistema de IoT en relacion para mejorar el control del clima en los cultivos de tomate chonto en la región, este se encargara de mantener una temperatura y humedad de tierra y suelo constantes, ademas de unos niveles de luminosidad, co2, y tiempos de riego automatizados, con la adicion de alertas para el ph del agua de fertiriego y los niveles de nutrientes en tierra. Este pretende mejorar la autonomia del invernadero y eliminar el error humano, ademas de las acciones que no tienen la capacidad de ser inmediatas debido al clima cambiante caracteristico de la región.

TomaCare
"Una nueva forma de cultivar"

Creado especificamente para agricultores medios Colombianos de la region risaraldense4

Componentes y tecnologias:

Sensores

DHT22 
Sensor capacitivo (SEN0193) 
BH1750 
MQ135 
SEN0161 + sonda 
Sensor multiparamétrico RS485 

Actuadores

2x bomba de agua sumergible 12V DC  — MOSFET IRF540N
Ventilador y extractor 24V DC — relé 2 canales + convertidor XL6009
Calefactor (resistencia PTC 12V) — MOSFET IRF540N
Humidificador ultrasónico 5V — relé 1 canal
Tiras LED de cultivo  — MOSFET IRFZ44N con PWM
Sistema de persianas con servomotores 
Buzzer activo y LEDs de alerta

Procesamiento

ESP32 

Software / Comunicación

Wi-Fi 
MQTT 
Modbus sobre RS485 
Arduino IDE / C++
Firebase 
HTML5
Agente inteligente basado en modelos (ABM)
  Sistema experto / basado en conocimiento 
  Red neuronal tipo perceptrón multicapa 
  Módulo de memoria 
Estado Del proyecto:

El proyecto aún está en desarrollo, pero debido a sus limitaciones de hardware, es imposible lanzar una demo real.
Todos los sensores y actuadores funcionan juntos, el siguiente paso es adaptarlo a una base de datos de Firebase.

Autor:

Esteban Castaño Castaño

==================================================================================

*English version*

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
