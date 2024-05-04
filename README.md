# CPE301_Final_Spring2024
You do not need to make any outer body for your cooler!!!

I just want to check your circuit and see for all the conditions, the stages are working properly or not. You can just give a sprinkle of water to check the water sensor or hold the temperature sensor to change the temperature value. 

 

FAQ:

Vent: You do not need to build any vent. Just activate your stepper motor when you are required to based on the project description. Add a piece of paper/wire/clip or anything with the motor so that we can see that the motor is moving.

Motors: Check the Motors slide for some ideas. For the fan motor, make sure to check the provided TinkerCad link and the circuit to work with the fan motor. I am not looking for any speed so you do not need to use the analogWrite function to manipulate the speed value. Instead of that you can connect the speedpin to the Vcc or make it HIGH to give it max speed. Check the tinkercad code/circuit and you will get some ideas.

ISR: Make sure to implement your own ISR for the start button. You can use attachInterrupt function for this reason. Check an example that's provided in the class module on canvas. For attachInterrupt() there are specific digital pins that will only work. Make sure to check the table here : https://www.arduino.cc/reference/en/language/functions/external-interrupts/attachinterrupt/

Links to an external site.

1-min Delay: ONLY for the 1-min delay you can use the millis() function https://docs.arduino.cc/built-in-examples/digital/BlinkWithoutDelay
Links to an external site. 
