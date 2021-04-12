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

using namespace std;
using namespace Pistache;

struct SmartWatch {
    int sleepHours;
    int sleepQuality;
    int heartRate;
    string wakeUpHour;
};

struct Ingredients {
    int sugarLvl;
    int coffeeLvl;
    int waterLvl;
    int milkLvl;
};

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

vector<Coffee> coffees;

namespace Generic {
    void handleReady(const Rest::Request&, Http::ResponseWriter response) {
        response.send(Http::Code::Ok, "Server is running");
    }
}

class CoffeeMakerEndpoint {
public:
    explicit CoffeeMakerEndpoint(Address addr)
            : httpEndpoint(std::make_shared<Http::Endpoint>(addr)) {}

    // Initialization of the server. Additional options can be provided here
    void init(size_t thr = 2) {
        auto opts = Http::Endpoint::options()
                .threads(static_cast<int>(thr));
        httpEndpoint->init(opts);

        setupRoutes();
    }

    // Server is started threaded.
    void start() {
        httpEndpoint->setHandler(router.handler());
        httpEndpoint->serveThreaded();
    }

    // When signaled server shuts down
    void stop() {
        httpEndpoint->shutdown();
    }

private:
    void setupRoutes() {
        using namespace Rest;
        Routes::Get(router, "/ready", Routes::bind(&Generic::handleReady));
        Routes::Post(router, "/settings/:settingName/:value", Routes::bind(&CoffeeMakerEndpoint::setSetting, this));
        Routes::Get(router, "/settings/:settingName/", Routes::bind(&CoffeeMakerEndpoint::getSetting, this));
    }

    void setSetting(const Rest::Request &request, Http::ResponseWriter response) {
        auto settingName = request.param(":settingName").as<std::string>();

        Guard guard(coffeeLock);

        string val = "";
        if (request.hasParam(":value")) {
            auto value = request.param(":value");
            val = value.as<string>();
        }

        int setResponse = cmk.set(settingName, val);

        // Send notifications if there aren't enough ingredients
        if (settingName.compare("chooseCoffee") == 0) {
            if (setResponse == 1){
                response.send(Http::Code::Ok, "Coffee is preparing");
            }
            else if (setResponse == -1) {
                response.send(Http::Code::Not_Found, "There isn't enough coffee");
            }
            else if (setResponse == -2) {
                response.send(Http::Code::Not_Found, "There isn't enough milk");
            }
            else if (setResponse == -3) {
                response.send(Http::Code::Not_Found, "There isn't enough sugar");
            }
            else if (setResponse == -4) {
                response.send(Http::Code::Not_Found, "There isn't enough water");
            }
        }
        else {
            if (setResponse == 1) {
                response.send(Http::Code::Ok, settingName + " was set to " + val);
            } else {
                response.send(Http::Code::Not_Found,
                            settingName + " was not found and or '" + val + "' was not a valid value ");
            }
        }
    }

    void getSetting(const Rest::Request &request, Http::ResponseWriter response) {
        auto settingName = request.param(":settingName").as<std::string>();

        Guard guard(coffeeLock);

        string valueSetting = cmk.get(settingName);

        if (valueSetting != "") {
            using namespace Http;
            response.headers()
                    .add<Header::Server>("pistache/0.1")
                    .add<Header::ContentType>(MIME(Text, Plain));

            response.send(Http::Code::Ok, settingName + " is " + valueSetting);
        } else {
            response.send(Http::Code::Not_Found, settingName + " was not found");
        }
    }

    class CoffeeMaker {
    private:
        bool cancelPrep;
        string showStage;
        SmartWatch smartData;
        vector<string> coffeeRecommendations;
        Ingredients ingredients;
        vector<pair<string, string>> history;
        string chooseCoffee;
    public:
        explicit CoffeeMaker() {
            cancelPrep = false;
            showStage = "none";
            chooseCoffee = "none";

            //Initial ingredients level from the coffee maker
            ingredients.coffeeLvl = 10;
            ingredients.milkLvl = 10;
            ingredients.sugarLvl = 10;
            ingredients.waterLvl = 10;
        };

        // Verify the level of ingredients and substract the necessary ingredients for the coffee
        int verifyIngredientsLevel(string coffeeName) {
            for (auto c : coffees) {
                if (c.name.compare(coffeeName) == 0) {
                    if (ingredients.coffeeLvl - c.coffeeNeeded < 0){
                        return -1;
                    }
                    else if (ingredients.milkLvl - c.milkNeeded < 0) {
                        return -2;
                    }
                    else if (ingredients.sugarLvl - c.sugarNeeded < 0) {
                        return -3;
                    }
                    else if (ingredients.waterLvl - c.waterNeeded < 0){
                        return -4;
                    }
                    else {
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

        int set(string name, string value) {
            if (name.compare("cancel") == 0){
                cancelPrep = true;
                return 1;
            }
            if (name.compare("showStage") == 0) {
                showStage = value;
                return 1;
            }
            if (name.compare("chooseCoffee") == 0){
                chooseCoffee = value;
                time_t now = time(0);
                // If there are enough ingredients, we prepare the coffee and append it to history
                if(verifyIngredientsLevel(value) == 1) {
                    history.push_back(make_pair(value, ctime(&now)));
                    return 1;
                }
                else {
                    return verifyIngredientsLevel(value);
                };
            }
            return 0;
        }

        string get(string name) {
            if (name.compare("cancel") == 0) {
                return to_string(cancelPrep);
            }
            if (name.compare("showStage") == 0) {
                return showStage;
            }
            if (name.compare("history") == 0) {
                // Show the history for the coffees
                string s = "";
                for (auto i: history) {
                    s.append(i.first + ", " + i.second + "\n");
                }
                return s;
            }
            if (name.compare("chooseCoffee") == 0) {
                return chooseCoffee;
            }
            if(name.compare("recommendations") == 0) {
                string s = "";
                for (auto i: coffeeRecommendations) {
                    s.append(i + "\n");
                }
                return s;
            }
            if(name.compare("ingredients") == 0) {
                string s = "";
                s.append("Coffee level: " + to_string(ingredients.coffeeLvl) + "\n");
                s.append("Water level: " + to_string(ingredients.waterLvl) + "\n");
                s.append("Sugar level: " + to_string(ingredients.sugarLvl) + "\n");
                s.append("Milk level: " + to_string(ingredients.milkLvl) + "\n");
                return s;
            }
            return "";
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
    // Scale from 1 to 5
    // Coffees available in the coffee maker
    coffees.push_back(Coffee("Espresso", 3, 0, 1, 0));
    coffees.push_back(Coffee("Latte", 3, 3, 2, 2));
    coffees.push_back(Coffee("Cappuccino", 3, 2, 1, 0));
    coffees.push_back(Coffee("Americano", 3, 0, 3, 1));
    coffees.push_back(Coffee("FlatWhite", 3, 1, 1, 1));

    // This code is needed for gracefull shutdown of the server when no longer needed.
    sigset_t signals;
    if (sigemptyset(&signals) != 0
        || sigaddset(&signals, SIGTERM) != 0
        || sigaddset(&signals, SIGINT) != 0
        || sigaddset(&signals, SIGHUP) != 0
        || pthread_sigmask(SIG_BLOCK, &signals, nullptr) != 0) {
        perror("install signal handler failed");
        return 1;
    }

    // Set a port on which your server to communicate
    Port port(9080);

    // Number of threads used by the server
    int thr = 2;

    if (argc >= 2) {
        port = static_cast<uint16_t>(std::stol(argv[1]));

        if (argc == 3)
            thr = std::stoi(argv[2]);
    }

    Address addr(Ipv4::any(), port);

    cout << "Cores = " << hardware_concurrency() << endl;
    cout << "Using " << thr << " threads" << endl;

    // Instance of the class that defines what the server can do.
    CoffeeMakerEndpoint stats(addr);

    // Initialize and start the server
    stats.init(thr);
    stats.start();

    // Code that waits for the shutdown sinal for the server
    int signal = 0;
    int status = sigwait(&signals, &signal);
    if (status == 0) {
        std::cout << "received signal " << signal << std::endl;
    } else {
        std::cerr << "sigwait returns " << status << std::endl;
    }

    stats.stop();
}