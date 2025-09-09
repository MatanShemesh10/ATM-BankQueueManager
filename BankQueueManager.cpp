#include "BankQueueManager.h"

int BankQueueManager::arrivalOrder = 0; // Definition and initialization outside the class


std::unique_ptr<Client> createClientFactory(const std::string& id,
                                     int balance,
                                     const std::string& typeStr)
{
    if (typeStr == "REGULAR") {
        return std::make_unique<RegularClient>(id, balance);
    } else if (typeStr == "VIP") {
        return std::make_unique<VipClient>(id, balance);
    } else if (typeStr == "BUSINESS") {
        return std::make_unique<BusinessClient>(id, balance);
    } else {
        std::cerr << "Unknown client type: " << typeStr << "\n";
        return nullptr;
    }
}

void BankQueueManager::createRequestFactory(const std::string& id,
                                            const std::string& service,
                                            int amount,
                                            const std::string& targetId) {
    Client* c = findClientById(id);
    if (!c) {
        std::cerr << "Client with ID " << id << " not found! skipping\n";
        return;
    }

    arrivalOrder++;

    std::unique_ptr<IServiceAction> newRequest;

    if (service == "withdraw") {
        newRequest = std::make_unique<WithdrawAction>(id, amount, c, arrivalOrder);
    } else if (service == "deposit") {
        newRequest = std::make_unique<DepositAction>(id, amount, c, arrivalOrder);
    } else if (service == "check") {
        newRequest = std::make_unique<CheckAction>(id, c, arrivalOrder);
    } else if (service == "transfer") {
        Client* to_client = findClientById(targetId);
        if (!to_client) {
            std::cerr << "Target client with ID " << id << " not found! skipping\n";
            return;
        }
        newRequest = std::make_unique<TransferAction>(id, amount, c, targetId, to_client, arrivalOrder);
    }

    AddRequestToQueue(std::move(newRequest));
}
                                   

void BankQueueManager::LoadPreClientsAndQueue()
{
    // load clients
    std::ifstream ClientsData("clients.json");
    if (!ClientsData) {
        std::cerr << "Failed to open file 'clients.json'.\n";
        return;
    }

    json j1;
    ClientsData >> j1; 

    for (const auto& clientJson : j1["clients"]) {
        std::string id;
        int balance;
        std::string typeStr;
        try {
            id = clientJson.at("id").get<std::string>();
            balance = clientJson.at("balance").get<int>();
            typeStr = clientJson.at("clientType").get<std::string>();

            auto client = createClientFactory(id, balance, typeStr);
            if (client) {
                clientsMap[id] = std::move(client);
            }
        } catch (const std::out_of_range& e) {
            std::cerr << "Missing required client field: " << e.what() << std::endl;
            continue;
        }
    }


    // load pre set queue
    std::ifstream QueueData("starting_queue.json");
    if (!QueueData) {
        std::cerr << "Failed to open file 'clients.json'.\n";
        return;
    }

    json j2;
    QueueData >> j2; 

    for (const auto& clientJson : j2["queue"]) {
        std::string id;
        std::string service;
        int amount = 0;
        std::string targetId;

        // Always required
        if (clientJson.contains("id")) {
            id = clientJson.at("id").get<std::string>();
        } else {
            std::cerr << "Missing 'id' field in queue entry.\n";
            continue;
        }
        if (clientJson.contains("service")) {
            service = clientJson.at("service").get<std::string>();
        } else {
            std::cerr << "Missing 'service' field in queue entry.\n";
            continue;
        }

        // Optional fields, depending on service
        if (clientJson.contains("amount")) {
            amount = clientJson.at("amount").get<int>();
        }
        if (clientJson.contains("targetId")) {
            targetId = clientJson.at("targetId").get<std::string>();
        }

        // std::cout << "id: " << id << ", service: " << service;
        // if (clientJson.contains("amount")) std::cout << ", amount: " << amount;
        // if (clientJson.contains("targetId")) std::cout << ", targetId: " << targetId;
        // std::cout << std::endl;

        createRequestFactory(id, service, amount, targetId);
    }

}

Client* BankQueueManager::findClientById(const std::string& id) {
    auto it = clientsMap.find(id);
    if (it != clientsMap.end()) {
        return it->second.get(); // מקבל מצביע מתוך unique_ptr
    }
    return nullptr;
}

void BankQueueManager::AddRequestToQueue(std::unique_ptr<IServiceAction> newRequest)
{
    
    Client* client = newRequest->getClient();  

    auto [it, inserted] = queue.insert(std::move(newRequest));

    if (inserted) {
        std::cout << "Added client '" << client->getId() << "' to service queue\n";
        ClientIdToQueueMap[client->getId()] = it;
    } else {
        std::cout << "Client '" << client->getId() << "' already inside the service queue - skipped.\n";
    }

}

void BankQueueManager::printQueue() {

    if (!queue.empty())
    {
        std::cout << "Bank queue:" << std::endl;
        for (const auto& actionPtr : queue) {
            Client* c = actionPtr->getClient();
            std::cout << "Id: " << c->getId() << ", Balance: " << c->getBalance() << ", Client type: " << c->getTypeAsString()
                      << ", Action Type: " <<  service_to_string(actionPtr->getServiceKind())
                      << ", Ticket #: " << actionPtr->getArrivalTicketNumber() << std::endl;
        }
    }
    else
    {
        std::cout << "Bank queue is empty" << std::endl;
    }
}

void BankQueueManager::printBankClients()
{
    if (!clientsMap.empty())
    {
        std::cout << "Bank clients:" << std::endl;
        for (const auto& pair : clientsMap)
        {
            const auto& clientPtr = pair.second;
            if (clientPtr) {
                std::cout << "Id: " << clientPtr->getId() << ", Balance: " << clientPtr->getBalance() << " , Client type: " << clientPtr->getTypeAsString() << std::endl;
            }
        }
    }
    else
    {
        std::cout << "There are no bank clients." << std::endl;
    }
}

void BankQueueManager::serveNext()
{
    if (!queue.empty())
    {
        auto qit = queue.begin();              
        IServiceAction* action = qit->get();   
    
        std::string cid = action->getClient()->getId();
    
        action->execute();
    
        queue.erase(qit);
        ClientIdToQueueMap.erase(cid);  
    }
    else
    {
        std::cout << "Bank queue is empty" << std::endl;
    }


}

void BankQueueManager::cancelClient(const std::string& id)
{

    auto it = ClientIdToQueueMap.find(id);
    if (it != ClientIdToQueueMap.end()) {
        queue.erase(it->second); // remove from the queue
        ClientIdToQueueMap.erase(it); // remove from the map
        std::cout << "Client '" << id << "' removed from the queue (canceled)" << std::endl;
    }
    else
    {
        std::cout << "No client found with id: '" << id << "' to cancel" << std::endl;
    }
}

void BankQueueManager::runCommand(const std::string& input)
{
    std::istringstream iss(input);
    std::vector<std::string> tokens;
    std::string word;

    while (iss >> word) {
        tokens.push_back(word);
    }

    if (tokens.empty()) return;

    Command cmd = parseCommand(tokens[0]);

    std::string id, service, targetId;
    int amount = -1;

    switch (cmd) 
    {
        case Command::ADD:
            if (tokens.size() < 3 || tokens.size() > 5) 
            {
                std::cout << "Invalid usage. Use: add [id] [service] [(optional)amount] [(optional)target id] " << std::endl;
                break;
            }

            try
            {
                id = tokens[1];
                service = tokens[2];
                
                if (tokens.size() >= 4) {
                    amount = std::stoi(tokens[3]);
                }

                if (tokens.size() == 5) {
                    targetId = tokens[4];
                }

                createRequestFactory(id,service,amount,targetId);
            }
            catch (const std::exception& e) 
            {
                std::cout << "Invalid input. Please enter a valid params. add [id (1 word string)] [service (1 word string)] [(optional)amount] [(optional)target id]" << std::endl;
            }
            break;

        case Command::CANCEL:
            if (tokens.size() != 2) 
            {
                std::cout << "Invalid usage. Use: cancel [id]" << std::endl;
                break;
            }

            try
            {
                id = tokens[1];
                cancelClient(id);
            }
            catch (const std::exception& e) 
            {
                std::cout << "Invalid input. Please enter a valid params. cancel [id (1 word string)]" << std::endl;
            }
            break;

        case Command::SERVE:
            serveNext();
            break;

        case Command::PRINTQ:
            printQueue();
            break;

        case Command::PRINTC:
            printBankClients();
            break;

        case Command::EXIT:
            std::cout << "Goodbye!\n";
            exit(0);

        case Command::UNKNOWN:
        default:
            std::cout << "Unknown command: " << tokens[0] << "\n";
            break;
    }
}



int main() {

    BankQueueManager manager;
    manager.LoadPreClientsAndQueue();
    std::cout << std::endl;
    manager.printBankClients();
    std::cout << std::endl;
    manager.printQueue();
    std::cout << std::endl;

    std::string input;
    
    std::cout << "Please insert one of the following commands:" << std::endl;
    std::cout << "add [id (1 word string)] [service (1 word string)] [(optional)amount] [(optional)target id]" << std::endl;
    std::cout << "cancel [id (1 word string)]" << std::endl;
    std::cout << "serve" << std::endl;
    std::cout << "printq (print queue)" << std::endl;
    std::cout << "printc (print bank clients)" << std::endl;
    std::cout << "exit" << std::endl;
    std::cout << std::endl;

    while (true) {
            std::cout << std::endl;
            std::cout << ">> ";
            std::getline(std::cin, input); // input from user
        
            manager.runCommand(input);
        
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    return 0;
}
