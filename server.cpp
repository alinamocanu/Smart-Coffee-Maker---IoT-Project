#include <algorithm>

#include <pistache/net.h>
#include <pistache/http.h>
#include <pistache/peer.h>
#include <pistache/http_headers.h>
#include <pistache/cookie.h>
#include <pistache/router.h>
#include <pistache/endpoint.h>
#include <pistache/common.h>

#include <signal.h>
#include <vector>

using namespace std;

struct SmartWatch {
    int sleepHours;
    int sleepQuality;
    int heartRate;
    string wakeUpHour;

};
struct Ingredients {
    int sugarLvl;
//    int coffeeLvl;
    int waterLvl;
    int milkLvl;
};


using namespace std;
using namespace Pistache;

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

    std::shared_ptr<Http::Endpoint> httpEndpoint;
    Rest::Router router;
private:

    void setupRoutes() {
        using namespace Rest;
        Routes::Get(router, "/cancel", Routes::bind(&CoffeeMakerEndpoint::cancelPreparation, this));
        Routes::Get(router, "/showStage", Routes::bind(&CoffeeMakerEndpoint::showStage, this));
        Routes::Get(router, "/alert", Routes::bind(&CoffeeMakerEndpoint::showAlert, this));
        Routes::Get(router, "/history", Routes::bind(&CoffeeMakerEndpoint::showHistory, this));
        Routes::Get(router, "/chooseCoffee", Routes::bind(&CoffeeMakerEndpoint::chooseCoffee, this));
        Routes::Get(router, "/recommendations", Routes::bind(&CoffeeMakerEndpoint::showRecommendations, this));
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

        if (setResponse == 1) {
            response.send(Http::Code::Ok, settingName + " was set to " + val);
        } else {
            response.send(Http::Code::Not_Found,
                          settingName + " was not found and or '" + val + "' was not a valid value ");
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

    void cancelPreparation(const Rest::Request &request, Http::ResponseWriter response) {
        response.send(Http::Code::Ok, "am ajuns la destinatie");

    }

    int showStage(const Rest::Request &request, Http::ResponseWriter response) {
        return 0;

    }

    int showAlert(const Rest::Request &request, Http::ResponseWriter response) {
        return 0;

    }

    int showHistory(const Rest::Request &request, Http::ResponseWriter response) {
        return 0;

    }

    int chooseCoffee(const Rest::Request &request, Http::ResponseWriter response) {
        return 0;

    }

    int showRecommendations(const Rest::Request &request, Http::ResponseWriter response) {
        return 0;

    }

    class CoffeeMaker {
    private:
        bool cancelPrep;
        string showStage;
        SmartWatch smartData;
        vector<string> coffeeRecommendations;
        bool alert;
        Ingredients ingredients;
        vector<string> history;
        string chooseCoffee;

    public:
        explicit CoffeeMaker() {};

        int set(string name, string value) {
            if (name == "cancel") {
                cancelPrep = true;
                return 1;
            } else if (name == "showStage") {
                showStage = value;
                return 1;

            } else if (name == "history") {
                history.push_back(value);
                return 1;
            }
//            else if(name == " "){
////                recomandation method setRecom
//            }
            return 0;

        }

        string get(string name) {
            if (name == "cancel") {
                return to_string(cancelPrep);
            }
            if (name == "showStage") {
                return showStage;
            }
            if (name == "history") {
                string s = "";
                for (auto i: history) {
                    s.append(i + ", ");
                }
                return s;
            }
            if (name == "chooseCoffee") {
                return chooseCoffee;
            }
            if (name == "alert") {
                return to_string(alert);
            }
            if (name == "recommendations") {
                string s = "";
                for (auto i: coffeeRecommendations) {
                    s.append(i + ", ");
                }
                return s;
            }using Lock = std::mutex;
            using Guard = std::lock_guard<Lock>;
            Lock coffeeLock;
            CoffeeMaker cmk;

        }

    };

    using Lock = std::mutex;
    using Guard = std::lock_guard<Lock>;
    Lock coffeeLock;
    CoffeeMaker cmk;

};

int main(int argc, char *argv[]) {

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