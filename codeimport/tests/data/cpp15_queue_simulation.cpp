#include <cstdlib>
#include <ctime>
#include <iostream>
#include <queue>
#include <string>

enum class Event { Arrival, Service, Idle };

struct Customer
{
    int id;
    int arrivalTime;
};

Event nextEvent(int tick)
{
    int r = std::rand() % 10;
    if (r < 3)
        return Event::Arrival;
    else if (r < 7 || tick % 5 == 0)
        return Event::Service;
    return Event::Idle;
}

std::string toString(Event e)
{
    switch (e) {
    case Event::Arrival: return "arrival";
    case Event::Service: return "service";
    case Event::Idle: return "idle";
    }
    return "?";
}

int main()
{
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    std::queue<Customer> q;
    int nextId = 1, served = 0, totalWait = 0;
    for (int tick = 0; tick < 50; tick++) {
        Event e = nextEvent(tick);
        switch (e) {
        case Event::Arrival:
            q.push({nextId++, tick});
            break;
        case Event::Service:
            if (q.empty())
                break;
            totalWait += tick - q.front().arrivalTime;
            q.pop();
            served++;
            break;
        default:
            break;
        }
        if (tick % 10 == 0)
            std::cout << "t=" << tick << " " << toString(e) << " queue=" << q.size() << std::endl;
    }
    std::cout << "served " << served << ", average wait "
              << (served ? static_cast<double>(totalWait) / served : 0.0) << std::endl;
    return 0;
}
