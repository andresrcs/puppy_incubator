# Puppy Incubator
If you have a new litter and you are in need of a puppy incubator but you don't want to spend thousands of dollars on a commercial one, this project is for you.

This incubator has automatic temperature control, using DS18B20 temperature sensors and a heat source (IR lamp, or even a regular incandescent bulb) activated through a relay accordingly to rules set by you. Also, you can monitor the current temperature and humidity accessing the IP of the D1 mini Pro board (it has a webserver) or log the data to a database with an ETL process since it also provides JSON output, and, if for some reason the temperature goes out of control (based on preset limits) it will send a `Push` notification to all your connected devices using IFTTT service.

Items used:
* X1 - Big enough plastic container
* X1 - Light socket
* X1 - Heat lamp
* X1 - Lolin D1 mini Pro board
* X1 - Tripler Base from Wemos
* X2 - DS18B20 probe temperature sensors
* X1 - DS18B20 shield from Wemos
* X1 - DHT11 shield from Wemos
* X1 - Relay shield from Wemos

This is the incubator:
![incubator](figures/incubator.jpg)
This is the real-time data shown on an HTML page:
![page](figures/html_page.png)
And this graphic was made with data logged from the sensor into a PostgreSQL database:
![plot](figures/plot.png)

## Further development
Next steps to improve this project would be:
* Adding a FAN to lower temperature or decrease the humidity inside the incubator.
* Adding a humidifier to control humidity levels inside the incubator.
* Adding an IP camera with a separate ESP32 modulo to remotely check on the puppies.

