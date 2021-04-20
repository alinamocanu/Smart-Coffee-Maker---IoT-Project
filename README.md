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

## Compilation

```bash

g++ smartCoffeeMaker.cpp -o coffeeMaker -lpistache -lcrypto -lssl -lpthread

```
To start server:

```bash
./coffeeMaker
```
To test, in another terminal type:

```bash
curl http://localhost:9080/ready
```

For post methods:

```bash
curl -X -POST [URL]
```
Or you can use a tool like [Postman](https://www.postman.com/)
Functionalities and analysis report - [Raport de analiza](https://github.com/AlinaMocanu/Smart-Coffee-Maker---IoT-Project/blob/main/Raport%20de%20analiza.docx)
