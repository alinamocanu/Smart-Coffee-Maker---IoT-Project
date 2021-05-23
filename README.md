# Smart Coffee Maker - University project

The purpose of this project is to simulate the functionalities of a smart coffee maker.

## Setup
Install [Pistache](http://pistache.io/) on Linux. Pistache doesn't support Windows, but you can use something like WSL or a virtual machine with Linux.

```bash

sudo add-apt-repository ppa:pistache+team/unstable
sudo apt update
sudo apt install libpistache-dev

```
You will also need C++ compiler, g++. To install, write the following command.

```bash

sudo apt install g++

```

Install Mosquitto library for C++ to use MQTT protocol:

```bash

sudo apt-get install mosquitto
sudo apt-get install mosquitto-clients

```

## Compilation

```bash

g++ smartCoffeeMaker.cpp -o coffeeMaker -lpistache -lcrypto -lssl -lpthread -lmosquitto

```

## Start MQTT process

```bash

mosquitto -v

```

## Start server:

```bash

./coffeeMaker

```

## Subscribe to MQTT topic 

```bash

mosquitto_sub -t mqtt

```

To test, in another terminal type:

```bash

curl http://localhost:9080/ready

```

For post methods:

```bash

curl -X POST [URL]

```
Or you can use a tool like [Postman](https://www.postman.com/)

Functionalities and analysis report - [Raport de analiza](https://github.com/AlinaMocanu/Smart-Coffee-Maker---IoT-Project/blob/mainRaport%20de%20analiza.docx)

## Features

1. Check ingredients level:

```bash

curl http://localhost:9080/settings/ingredients

```

2. Choose and prepare a coffee:

```bash

curl -X POST http://localhost:9080/settings/chooseCoffee/Espresso

```

3. Cancel coffee preparation:

```bash

curl -X POST http://localhost:9080/settings/cancel/0

```

4. Make coffee recommendations based on SmartWatch values:

```bash

curl -X POST http://localhost:9080/settings/recommendations/6,70,80,10

```

5. Check history of prepared coffees(or see the historyFile.txt):

```bash

curl http://localhost:9080/settings/history

```
