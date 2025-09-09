#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <set>
#include <vector>
#include <sstream>
#include <algorithm>
#include <unordered_map>
#include <fstream>
#include "include/json.hpp"
using json = nlohmann::json;

enum class ClientType {
    VIP,
    BUSINESS,
    REGULAR,
    UNKNOWN
};

enum class Command {
    ADD,
    CANCEL,
    SERVE,
    PRINTQ,
    PRINTC,
    EXIT,
    UNKNOWN
};

enum class Service { // todo - write in capital letters!
    DEPOSIT ,
    WITHDRAW ,
    CHECK ,
    TRANSFER,
    UNKNOWN
};

enum class ClientPriority {
    UNKNOWN = 0,
    REGULAR = 1,
    BUSINESS = 2,
    VIP = 3
};

ClientType parseClientType(const std::string& str) {
    if (str == "REGULAR") return ClientType::REGULAR;
    if (str == "VIP") return ClientType::VIP;
    if (str == "BUSINESS") return ClientType::BUSINESS;
    return ClientType::UNKNOWN;
}

Service parseService(const std::string& service) {
    if (service == "deposit")    return Service::DEPOSIT;
    if (service == "withdraw")    return Service::WITHDRAW;
    if (service == "check")  return Service::CHECK;
    if (service == "transfer")  return Service::TRANSFER;
    return Service::UNKNOWN;
}

inline std::string service_to_string(Service k) {
    switch (k) {
        case Service::WITHDRAW: return "withdraw";
        case Service::DEPOSIT:  return "deposit";
        case Service::CHECK:    return "check";
        case Service::TRANSFER: return "transfer";
    }
    return "unknown";
}

Command parseCommand(const std::string& cmd) {
    if (cmd == "add")    return Command::ADD;
    if (cmd == "cancel")    return Command::CANCEL;
    if (cmd == "serve")  return Command::SERVE;
    if (cmd == "printq")  return Command::PRINTQ;
    if (cmd == "printc")  return Command::PRINTC;
    if (cmd == "exit")   return Command::EXIT;
    return Command::UNKNOWN;
}

class Client {
    public:
        Client(std::string id_, int balance_)
            : id(std::move(id_)), balance(balance_) {}

        std::string getId() const { return id; }
        int getBalance() const { return balance; }
        virtual ClientType getType() const = 0;
        virtual ~Client() = default;


        std::string getTypeAsString() const {
            switch (getType()) {
                case ClientType::REGULAR: return "Regular";
                case ClientType::VIP: return "VIP";
                case ClientType::BUSINESS: return "Business";
                default: return "UNKNOWN";
            }
        }

        bool deposit(int amount) 
        {
            if (amount <= 0) return false;
            if (balance > INT_MAX - amount) return false; // overflow guard
            balance += amount;
            return true;
        }

        bool withdraw(int amount) 
        {
            if (amount <= 0) return false;
            if (balance < amount) return false; 
            balance -= amount;
            return true;
        }

    protected:
        std::string id;
        int balance;

};

inline bool transfer_atomic(Client& from, Client& to, int amount) {
    if (amount <= 0) return false;
    if (!from.withdraw(amount)) return false;
    if (!to.deposit(amount)) {                          
        from.deposit(amount); 
        return false;
    }
    return true;
}

class RegularClient : public Client {
    public:
        RegularClient(std::string id_, int balance_)
            : Client(std::move(id_), balance_) {}

        ClientType getType() const override {
            return ClientType::REGULAR;
        }
};

class VipClient : public Client {
    public:
        VipClient(std::string id_, int balance_)
            : Client(std::move(id_), balance_) {}

        ClientType getType() const override {
            return ClientType::VIP;
        }
};

class BusinessClient : public Client {
    public:
        BusinessClient(std::string id_, int balance_)
            : Client(std::move(id_), balance_) {}

        ClientType getType() const override {
            return ClientType::BUSINESS;
        }
};

class IServiceAction {
protected:
    std::string id;
    Client* client;
    int arrivalTicketNumber;

public:
    IServiceAction(const std::string& id, Client* client, int arrivalTicketNumber)
        : id(id), client(client), arrivalTicketNumber(arrivalTicketNumber) {}

    Client* getClient() const {
        return client;
    }

    virtual Service getServiceKind() const noexcept = 0;

    int getArrivalTicketNumber() const {
        return arrivalTicketNumber;
    }

    virtual void execute() = 0;
    virtual ~IServiceAction() = default;
};

// --- Withdraw ---
class WithdrawAction : public IServiceAction {
    int amount;

public:
    WithdrawAction(const std::string& id, int amt, Client* client, int arrivalTicketNumber)
        : IServiceAction(id, client, arrivalTicketNumber), amount(amt) {}

    Service getServiceKind() const noexcept override { return Service::WITHDRAW; }

    void execute() override 
    {
        if (!client->withdraw(amount)) 
        {
            std::cout << "Withdraw failed for client " << client->getId()
                      << " (invalid amount or insufficient funds)\n";
            return;
        }
        std::cout << "Withdrew " << amount << "$ by client '" << client->getId()
                  << "' | client new balance: " << client->getBalance() << "$\n";
    }
};

// --- Deposit ---
class DepositAction : public IServiceAction {
    int amount;

public:
    DepositAction(const std::string& id, int amt, Client* client, int arrivalTicketNumber)
        : IServiceAction(id, client, arrivalTicketNumber), amount(amt) {}

    Service getServiceKind() const noexcept override { return Service::DEPOSIT; }

    void execute() override 
    {
        if (!client->deposit(amount)) 
        {
            std::cout << "Deposit failed for client " << client->getId()
                      << " (invalid amount or overflow)\n";
            return;
        }
        std::cout << "Deposited " << amount << "$ to client '" << client->getId()
                  << "' | client new balance: " << client->getBalance() << "$\n";
    }
};

// --- Check ---
class CheckAction : public IServiceAction {
public:
    CheckAction(const std::string& id, Client* client, int arrivalTicketNumber)
        : IServiceAction(id, client, arrivalTicketNumber) {}

    Service getServiceKind() const noexcept override { return Service::CHECK; }

    void execute() override {
        std::cout << "Client '" << client->getId()
                  << "' have balance of " << client->getBalance() << "$\n";
    }
};

// --- Transfer ---
class TransferAction : public IServiceAction {
    std::string targetId;
    Client* to_client;
    int amount;

public:
    TransferAction(const std::string& id,
                   int amt,
                   Client* client,
                   const std::string& target,
                   Client* to_client,
                   int arrivalTicketNumber)
        : IServiceAction(id, client, arrivalTicketNumber),
          targetId(target), to_client(to_client), amount(amt) {}

    Service getServiceKind() const noexcept override { return Service::TRANSFER; }

    void execute() override 
    {
        if (!transfer_atomic(*client, *to_client, amount)) 
        {
            std::cout << "Transfer failed: " << client->getId()
                      << " -> " << to_client->getId()
                      << " amount=" << amount << "\n";
            return;
        }
        std::cout << "Transferred " << amount << "$ from client '" << client->getId()
                  << "' to client '" << to_client->getId()
                  << "' | new balances: client '" << client->getId() << "' : " << client->getBalance()
                  << "$ , client '" << to_client->getId() << "' : " << to_client->getBalance() << "$ \n";
    }
};

struct IServiceActionComparator {
    bool operator()(const std::unique_ptr<IServiceAction>& a,
                    const std::unique_ptr<IServiceAction>& b) const
    {
        if (!a || !b) return a < b;

        Client* ca = a->getClient();
        Client* cb = b->getClient();

        if (!ca || !cb) return ca < cb;

        ClientType ta = ca->getType();
        ClientType tb = cb->getType();

        if (ta != tb)
            return ta < tb; // VIP (0) < BUSINESS (1) < REGULAR (2)

        // סוגים שווים – השווה לפי arrivalTicketNumber
        return a->getArrivalTicketNumber() < b->getArrivalTicketNumber();
    }
};

using QueueIt = std::set<std::unique_ptr<IServiceAction>, IServiceActionComparator>::iterator;

class BankQueueManager 
{
    public:
        void runCommand(const std::string& input); 
        void LoadPreClientsAndQueue();
        void printBankClients();
        void printQueue();
        
        private: 
        
        std::unordered_map<std::string, std::unique_ptr<Client>> clientsMap;
        std::set<std::unique_ptr<IServiceAction>, IServiceActionComparator> queue;
        std::unordered_map<std::string, QueueIt> ClientIdToQueueMap;
        static int arrivalOrder; // Declaration only
        
        void addClient(const std::string& id, const std::string& service, int priority);
        void serveNext();
        void cancelClient(const std::string& id);
        Client* findClientById(const std::string& id);
        void createRequestFactory(const std::string& id,
                                            const std::string& service,
                                            int amount,
                                            const std::string& targetId);
        void AddRequestToQueue(std::unique_ptr<IServiceAction> newRequest);
    };


