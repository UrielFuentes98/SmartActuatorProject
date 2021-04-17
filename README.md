# SmartActuatorProject

- Prototype for a digitally controlled linear actuator.
- Built using a STM32F103 as a base microcontroller and freeRTOS as an
RTOS.
- The system reads a linear potentiometer to get the position and uses a control algorithm to adjust it. 
- The system communicates through a CAN bus with other systems and a ECU that sends to it position commands.
