/*  
    Smart CoffeMaker

    Bajan Ioana
    Dinu Delia
    Dobre Beatrice
    Mocanu Alina
    
    using Rares Cristea's example, 12.03.2021
    using Mathieu Stefani's example, 07 février 2016
*/

#include <algorithm>
#include <pistache/net.h>
#include <pistache/http.h>
#include <pistache/peer.h>
#include <pistache/http_headers.h>
#include <pistache/cookie.h>
#include <pistache/router.h>
#include <pistache/endpoint.h>
#include <pistache/common.h>
#include <ctime>
#include <signal.h>
#include <vector>
#include <stdio.h>
#include <chrono>
#include <thread>
#include <iostream>
#include <fstream>
#include <mosquitto.h>

using namespace std;
using namespace Pistache;

struct SmartWatch {
    float sleepHours;
    float sleepQuality;
    float heartRate;
    float wakeUpHour;
};

struct Ingredients {
    int sugarLvl;
    int coffeeLvl;
    int waterLvl;
    int milkLvl;
};

// level of ingredients needed for a specific type of coffee
class Coffee {
public:
    int milkNeeded;
    int waterNeeded;
    int sugarNeeded;
    int coffeeNeeded;
    string name;

    Coffee(string n, int c, int m, int w, int s) {
        milkNeeded = m;
        waterNeeded = w;
        sugarNeeded = s;
        name = n;
        coffeeNeeded = c;
    }
};

// coffees available in coffee maker
vector<Coffee> coffees;
struct mosquitto* _mosq = nullptr;

// this is just a helper function to preety-print the Cookies that one of the enpoints shall receive.
void printCookies(const Http::Request &req) {
    auto cookies = req.cookies();
    cout << "Cookies: [" << std::endl;
    const string indent(4, ' ');
    for (const auto &c: cookies) {
        cout << indent << c.name << " = " << c.value << endl;
    }
    cout << "]" << std::endl;
}

// helper to test if the server is functional
namespace Generic {

    void handleReady(const Rest::Request &, Http::ResponseWriter response) {
        response.send(Http::Code::Ok, "The Coffee Maker is functional!");
    }

}

// function used to parse a string 
vector<string> split(const string &s, char delimiter) {
    vector<string> tokens;
    string token;
    istringstream tokenStream(s);
    while (getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

class CoffeeMakerEndpoint {
public:
    explicit CoffeeMakerEndpoint(Address addr)
            : httpEndpoint(std::make_shared<Http::Endpoint>(addr)) {}

    // initialization of the server
    void init(size_t thr = 2) {
        auto opts = Http::Endpoint::options()
                .threads(static_cast<int>(thr));
        httpEndpoint->init(opts);

        // mosquitto protocol initialization
        _mosq = mosquitto_new(nullptr, true, nullptr);
        if (mosquitto_connect(_mosq, _host.c_str(), _port, _keep_alive) != MOSQ_ERR_SUCCESS) {
            cerr << "connect error!!" << endl;
        } else {
            _connected = true;
        }

        setupRoutes();
    }

    // server is started threaded with additional thread
    void start() {
        httpEndpoint->setHandler(router.handler());
        httpEndpoint->serveThreaded();

        checkLoopThread = new thread(&CoffeeMaker::checkData, ref(cmk));
    }

    // when signaled server shuts down
    void stop() {
        httpEndpoint->shutdown();

        cmk.runThread = false;

        checkLoopThread->join();
    }

private:
    thread *checkLoopThread;
    string _host = "localhost";
    int _port = 1883;
    int _keep_alive = 60;
    bool _connected = false;

    // http routes available 
    void setupRoutes() {
        using namespace Rest;
        Routes::Get(router, "/ready", Routes::bind(&Generic::handleReady));
        Routes::Get(router, "/auth", Routes::bind(&CoffeeMakerEndpoint::doAuth, this));
        Routes::Post(router, "/settings/:settingName/:value", Routes::bind(&CoffeeMakerEndpoint::setSetting, this));
        Routes::Get(router, "/settings/:settingName/", Routes::bind(&CoffeeMakerEndpoint::getSetting, this));
    }

    void doAuth(const Rest::Request &request, Http::ResponseWriter response) {
        // function that prints cookies
        printCookies(request);
        // in the response object, it adds a cookie regarding the communications language.
        response.cookies()
                .add(Http::Cookie("lang", "en-US"));
        // send the response
        response.send(Http::Code::Ok);
    }

    // set functionalities
    void setSetting(const Rest::Request &request, Http::ResponseWriter response) {
        auto settingName = request.param(":settingName").as<std::string>();

        Guard guard(coffeeLock);

        string val = "";
        if (request.hasParam(":value")) {
            auto value = request.param(":value");
            val = value.as<string>();
        }

        int setResponse = cmk.set(settingName, val);

        // send notifications if there aren't enough ingredients
        if (settingName.compare("chooseCoffee") == 0) {
            if (setResponse == 1) {
                response.send(Http::Code::Ok, "Coffee is preparing \n Stage: " + cmk.coffeeStage(cmk.getStage()));
            } else if (setResponse == -1) {
                response.send(Http::Code::Not_Found, "There isn't enough coffee");
            } else if (setResponse == -2) {
                response.send(Http::Code::Not_Found, "There isn't enough milk");
            } else if (setResponse == -3) {
                response.send(Http::Code::Not_Found, "There isn't enough sugar");
            } else if (setResponse == -4) {
                response.send(Http::Code::Not_Found, "There isn't enough water");
            }
        // cancel coffee preparation
        } else if (settingName.compare("cancel") == 0) {
            if (setResponse == 1) {
                response.send(Http::Code::Ok, "Cancel Preparation \n Stage: " + cmk.coffeeStage(cmk.getStage()));
            }
        // show the current stage of the coffee maker
        } else if (settingName.compare("showStage") == 0) {
            if (setResponse == 1) {
                response.send(Http::Code::Ok, "Stage : " + cmk.coffeeStage(cmk.getStage()));
            }
        } else if (settingName.compare("recommendations")) {
            if (setResponse == 1) {
                response.send(Http::Code::Ok, "Recommendations processing");
            }
        } else {
            if (setResponse == 1) {
                response.send(Http::Code::Ok, settingName + " was set to " + val);
            } else {
                response.send(Http::Code::Not_Found,
                              settingName + " was not found and or '" + val + "' was not a valid value ");
            }
        }
    }

    // get functionalities
    void getSetting(const Rest::Request &request, Http::ResponseWriter response) {
        auto settingName = request.param(":settingName").as<std::string>();

        Guard guard(coffeeLock);

        string valueSetting = cmk.get(settingName);

        if (valueSetting != "") {
            using namespace Http;
            response.headers()
                    .add<Header::Server>("pistache/0.1")
                    .add<Header::ContentType>(MIME(Text, Plain));

            response.send(Http::Code::Ok, settingName + " :\n" + valueSetting);
        } else {
            response.send(Http::Code::Not_Found, settingName + " was not found");
        }
    }

    class CoffeeMaker {
    private:
        bool cancelPrep;
        int showStage;
        string chooseCoffee;
        SmartWatch sm;
        Ingredients ingredients;
        vector<string> coffeeRecommendations;
        vector<pair<string, string>> history;
    public:
        bool runThread = true;
        bool recommendationsMade = false;

        explicit CoffeeMaker() {
            cancelPrep = false;
            showStage = 0;
            chooseCoffee = "none";

            // initial ingredients level from the coffee maker
            ingredients.coffeeLvl = 10;
            ingredients.milkLvl = 10;
            ingredients.sugarLvl = 10;
            ingredients.waterLvl = 10;

            // initial values from the SmartWatch
            sm.heartRate = -1;
            sm.sleepHours = -1;
            sm.sleepQuality = -1;
            sm.wakeUpHour = -1;
        };

        // available stages for the coffee maker
        static string coffeeStage(int p) {
            switch (p) {
                case 0:
                    return "Idle";
                case 1:
                    return "Your coffee is in the making!";
                case 2:
                    return "Your coffee is ready";
                case 3:
                    return "Coffee canceled";
                default:
                    return "Oopsie! Unkown stage :(";
            }
        }

        int getStage() {
            return showStage;
        }

        // verify the level of ingredients and substract the necessary ingredients for the coffee
        int verifyIngredientsLevel(string coffeeName) {
            for (auto c : coffees) {
                if (c.name.compare(coffeeName) == 0) {
                    if (ingredients.coffeeLvl - c.coffeeNeeded < 0) {
                        return -1;
                    } else if (ingredients.milkLvl - c.milkNeeded < 0) {
                        return -2;
                    } else if (ingredients.sugarLvl - c.sugarNeeded < 0) {
                        return -3;
                    } else if (ingredients.waterLvl - c.waterNeeded < 0) {
                        return -4;
                    } else {
                        ingredients.coffeeLvl -= c.coffeeNeeded;
                        ingredients.milkLvl -= c.milkNeeded;
                        ingredients.sugarLvl -= c.sugarNeeded;
                        ingredients.waterLvl -= c.waterNeeded;
                        return 1;
                    }
                }
            }
            return 0;
        }

        // modify SmartWatch values from the request
        void modifySMData(vector<string> smartWatchVal) {
            sm.sleepHours = stoi(smartWatchVal[0]);
            sm.sleepQuality = stoi(smartWatchVal[1]);
            sm.heartRate = stoi(smartWatchVal[2]);
            sm.wakeUpHour = stoi(smartWatchVal[3]);
        }

        // calculate coffee recommendations
        void makeRecommendations() {
            double score;
            coffeeRecommendations.clear();
            score = (sm.sleepHours * 7 + sm.sleepQuality * 3) / 10;

            if (score < 20) {
                if (sm.heartRate > 100) {
                    coffeeRecommendations.push_back(coffees[7].name);
                    coffeeRecommendations.push_back(coffees[0].name);
                    coffeeRecommendations.push_back(coffees[1].name);
                } else {
                    coffeeRecommendations.push_back(coffees[6].name);
                    coffeeRecommendations.push_back(coffees[7].name);
                    coffeeRecommendations.push_back(coffees[8].name);
                }
            } else if (score < 35) {
                coffeeRecommendations.push_back(coffees[3].name);
                coffeeRecommendations.push_back(coffees[4].name);
                coffeeRecommendations.push_back(coffees[5].name);
            } else {
                coffeeRecommendations.push_back(coffees[0].name);
                coffeeRecommendations.push_back(coffees[1].name);
                coffeeRecommendations.push_back(coffees[2].name);
            }
        }

        int set(string name, string value) {
            if (name.compare("cancel") == 0) {
                cancelPrep = true;
                showStage = 3;
                return 1;
            }

            if (name.compare("showStage") == 0) {
                return 1;
            }

            if (name.compare("chooseCoffee") == 0) {
                chooseCoffee = value;
                showStage = 1;
                time_t now = time(0);
                // if there are enough ingredients, we prepare the coffee and append it to history
                if (verifyIngredientsLevel(value) == 1) {
                    history.push_back(make_pair(value, ctime(&now)));
                    showStage = 2;
                    return 1;
                } else {
                    // notification for lack of ingredients
                    return verifyIngredientsLevel(value);
                }
            }

            if (name.compare("recommendations") == 0) {
                vector<string> smartWatchVal = split(value, ',');
                if (smartWatchVal.size() != 4) {
                    return 0;
                }
                
                modifySMData(smartWatchVal);
                return 1;
            }

            return 0;
        }

        ofstream MyOutputFile;
        

        string get(string name) {
            if (name.compare("cancel") == 0) {
                return to_string(cancelPrep);
            }

            // publish stage of the coffee maker on mqtt
            if (name.compare("showStage") == 0) {
                if (mosquitto_publish(_mosq, 
                            nullptr, 
                            "mqtt",
                            static_cast<int>(coffeeStage(getStage()).length()) + 1,
                            coffeeStage(getStage()).c_str(), 0, 0) != MOSQ_ERR_SUCCESS) {

                    std::cerr<< "MQTT publish error." << std::endl;
                    return "";
                }
               
                return coffeeStage(getStage());
            }

            if (name.compare("history") == 0) {
                // show the history for the coffees and write it on the output file
                string s = "";
                MyOutputFile.open("historyFile.txt");
                for (auto i: history) {
                    s.append(i.first + ", " + i.second + "\n");
                    MyOutputFile<<i.first + ", " + i.second + "\n";
                }

                MyOutputFile.close();
               
                return s;
            }

            if (name.compare("chooseCoffee") == 0) {
                return chooseCoffee;
            }

            if (name.compare("recommendations") == 0) {
                string s = "";
                for (auto i: coffeeRecommendations) {
                    s.append(i + "\n");
                }
                return s;
            }

            if (name.compare("ingredients") == 0) {
                string s = "";
                s.append("Coffee level: " + to_string(ingredients.coffeeLvl) + "\n");
                s.append("Water level: " + to_string(ingredients.waterLvl) + "\n");
                s.append("Sugar level: " + to_string(ingredients.sugarLvl) + "\n");
                s.append("Milk level: " + to_string(ingredients.milkLvl) + "\n");
                return s;
            }

            if (name.compare("smartwatch") == 0) {
                string s = "";
                s.append("Number of hours slept " + to_string(sm.sleepHours) + "\n");
                s.append("Sleep quality: " + to_string(sm.sleepQuality) + "\n");
                s.append("Heart rate: " + to_string(sm.heartRate) + "\n");
                s.append("Wake up hour: " + to_string(sm.wakeUpHour) + "\n");
                return s;
            }

            return "";
        }

        // thread to verify every 10 seconds if the user just woke up to make coffee recommendations
        void checkData() {
            while (runThread) {
                this_thread::sleep_for(chrono::seconds(10));
                time_t tt;
                time(&tt);
                tm TM = *localtime(&tt);
                float currentHour = TM.tm_hour;
                cerr<<"Local time:"<<currentHour;
                if (sm.heartRate != -1 && sm.sleepHours != -1 && sm.sleepQuality != -1 && sm.wakeUpHour != -1) {
                    if (sm.sleepHours >= 3 && currentHour - sm.wakeUpHour == 0 && recommendationsMade == false) {
                        recommendationsMade = true;
                        makeRecommendations();
                    } else if (currentHour - sm.wakeUpHour > 0) {
                        recommendationsMade = false;
                    }
                }
            }
        }
    };

    using Lock = std::mutex;
    using Guard = std::lock_guard<Lock>;
    Lock coffeeLock;

    CoffeeMaker cmk;

    std::shared_ptr<Http::Endpoint> httpEndpoint;
    Rest::Router router;
};

int main(int argc, char *argv[]) {
    // coffees available in the coffee maker
    // weak
    coffees.push_back(Coffee("Latte", 3, 3, 2, 2));
    coffees.push_back(Coffee("Cappuccino", 3, 2, 1, 0));
    coffees.push_back(Coffee("Moccaccino", 3, 3, 2, 2));
    // medium
    coffees.push_back(Coffee("Machiatto", 3, 3, 2, 2));
    coffees.push_back(Coffee("FlatWhite", 3, 1, 1, 1));
    coffees.push_back(Coffee("Curtado", 3, 1, 1, 1));
    // strong
    coffees.push_back(Coffee("Espresso", 3, 0, 1, 0));
    coffees.push_back(Coffee("Americano", 3, 0, 3, 1));
    coffees.push_back(Coffee("Turkish", 3, 0, 3, 1));

    // this code is needed for gracefull shutdown of the server when no longer needed.
    sigset_t signals;
    if (sigemptyset(&signals) != 0
        || sigaddset(&signals, SIGTERM) != 0
        || sigaddset(&signals, SIGINT) != 0
        || sigaddset(&signals, SIGHUP) != 0
        || pthread_sigmask(SIG_BLOCK, &signals, nullptr) != 0) {
        perror("install signal handler failed");
        return 1;
    }

    // set a port on which your server to communicate
    Port port(9080);

    // number of threads used by the server
    int thr = 2;

    if (argc >= 2) {
        port = static_cast<uint16_t>(std::stol(argv[1]));

        if (argc == 3)
            thr = std::stoi(argv[2]);
    }

    Address addr(Ipv4::any(), port);

    cout << "Cores = " << hardware_concurrency() << endl;
    cout << "Using " << thr << " threads" << endl;

    // instance of the class that defines what the server can do.
    CoffeeMakerEndpoint stats(addr);

    // initialize and start the server
    stats.init(thr);
    stats.start();

    // code that waits for the shutdown sinal for the server
    int signal = 0;
    int status = sigwait(&signals, &signal);
    if (status == 0) {
        mosquitto_disconnect(_mosq);
        mosquitto_lib_cleanup();
        mosquitto_destroy(_mosq);

        std::cout << "received signal " << signal << std::endl;
    } else {
        std::cerr << "sigwait returns " << status << std::endl;
    }

    stats.stop();
}