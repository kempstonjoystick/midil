# midil

Overview: 
midil will run on custom hardware (C.H.I.P Pro - https://getchip.com/pages/chippro). It will provide a lightweight/robust
alternative to using a PC to manage MIDI (Musical Instrument Digital Interface) devices. It also provides the ability
to directly interface USB midi endpoints with the traditional DIN-9 devices.


midil (WIP):
 - Utility that will allow simple, persistent routing of MIDI signals between a DIN MIDI connector (UART, 31250 baud) 
 and USB endpoints. Will also initialize and route TTY MIDI messages from a user configurable device into alse.

Additional apps (future work):
 - Flask web application to allow mapping to be defined using web GUI
 - Bluetooth smartphon app, as above.
 