# BankQueueManager
A compact, production-minded C++ demo that models a bank counter service queue.  
This repository is intentionally small but designed to show practical engineering decisions around ownership, polymorphism, deterministic scheduling, and efficient cancellation semantics. It is written to be interview-friendly: the files are focused, readable, and showcase real trade-offs.

---

## Quick summary
`BankQueueManager` models a bank counter queue where clients (Regular / VIP / Business) submit service requests (deposit, withdraw, check, transfer).  
The system uses a polymorphic action model (`IServiceAction`), strict ownership (`std::unique_ptr` referred to in the codebase as "SMRT PTR"), and a `std::set` ordered by a domain-aware comparator that ensures priority and deterministic FIFO within each priority level. To enable efficient cancellation we store the iterator returned by `set::insert` so removals are O(log n) rather than O(n).

---

## High-level architecture
- **Client model**: Abstract base `Client` with concrete subclasses `RegularClient`, `VipClient`, `BusinessClient`. Clients hold id and balance and expose domain operations such as `deposit()` and `withdraw()`. Client type is used by the comparator to decide priority ordering.
- **Action model**: `IServiceAction` is the polymorphic interface for queued actions. Each concrete action (`WithdrawAction`, `DepositAction`, `CheckAction`, `TransferAction`) implements `execute()` and contains a pointer to the owning `Client`, the arrival ticket, and the relevant parameters (amount, target client id, etc).
- **Queue**: `std::set<std::unique_ptr<IServiceAction>, IServiceActionComparator>` stores actions ordered by client priority and arrival ticket. Using `unique_ptr` in the set ensures clear ownership and simple move semantics.
- **Iterator cache**: When inserting into the set we save the returned iterator into `ClientIdToQueueMap` (an unordered_map from client id to set iterator). This is the key trick that yields O(log n) cancellation by id.
- **Factory functions**: Creation of `Client` subclasses and `IServiceAction` objects is centralized in factories that validate input and return `unique_ptr` instances. That keeps parsing and validation logic out of business paths.
- **CLI and JSON loader**: A small CLI loop allows adding, canceling, serving, printing queue and clients. The loader reads `clients.json` and `starting_queue.json` at startup to populate state.

---

## Repo layout
- `BankQueueManager.h` - core types, class declarations, comparator and aliases.  
- `BankQueueManager.cpp` - implementation, factory functions, JSON loading, CLI loop.  
- `clients.json` - sample client dataset used by the loader.  
- `starting_queue.json` - sample pre-seeded queue entries.  
- `README_short.md` - short README for GitHub landing page.  
- `CONTRIBUTING.md` - contribution guide and testing notes.  
- `LICENSE` - MIT license.  
- `demo-commands.txt` - CLI script to exercise common flows.  
- `CMakeLists.txt` - optional CMake helper for building and enabling tests.  
- `transfer_sequence_diagram.txt` - ASCII sequence diagram for a transfer action flow.
---

## Important implementation details and trade-offs
- **Ownership - SMRT PTR**: `std::unique_ptr` is used uniformly for entities that have single ownership semantics. This communicates intent clearly, eliminates double-free risks, and works well when moving objects into containers.
- **Set of unique_ptr**: Using a `std::set` of `unique_ptr` requires a comparator that dereferences the pointers for ordering. This is slightly unusual but gives ordered semantics with stable element addresses (iterators remain valid until erase) and integrates cleanly with the iterator caching approach.
- **Iterator caching for cancellation**: The common interview question is "how do you remove an arbitrary element from an ordered container efficiently?" We solve it by saving the iterator from `insert` and later erasing by that iterator. This avoids scanning or re-building the set.
- **Comparator is domain-aware**: `IServiceActionComparator` encodes business rules: compare client type first (VIP > BUSINESS > REGULAR), then arrival ticket. This keeps business logic in one place and makes it easy to change scheduling policies (for example to add waiting-time boosts or dynamic priority).
- **Atomic-like transfer with rollback**: `transfer_atomic` performs withdraw on the source and deposit on the target. If the deposit would fail (overflow or missing target), the function rolls back the withdrawal. This implements a lightweight, local consistency model appropriate for a single-threaded demo without introducing locks or a full transaction log.
- **Single-threaded model by design**: This project is intentionally single-threaded. Concurrency can be added, but it changes the reasoning around invariant preservation, iterator validity, and how transfer rollback should be implemented. Suggested extensions are below.
- **Defensive parsing and error handling**: JSON loading and CLI parsing check fields and print clear diagnostics on missing or malformed data.

---

## Key classes and methods (summary)
### Client hierarchy
- `Client` (abstract)
  - `id`, `balance`
  - `deposit(amount)`, `withdraw(amount)` - both return status codes / booleans and validate over/underflow
  - `getType()` - returns enum for comparator
- `RegularClient`, `VipClient`, `BusinessClient` - lightweight subclasses that only influence `getType()` and potentially domain-specific limits

### IServiceAction and concrete actions
- `IServiceAction` contains:
  - `ownerClientId` or pointer to owner client
  - `arrivalTicket` (monotonic integer)
  - `execute(BankQueueManager & mgr)` - polymorphic behavior
  - `getServiceKind()` - enum or string useful for logging
- `WithdrawAction` - calls `withdraw()` on owner and logs result
- `DepositAction` - calls `deposit()` on owner and logs result
- `CheckAction` - prints balance without modifying state
- `TransferAction` - coordinates `transfer_atomic(fromId, toId, amount)`

### BankQueueManager responsibilities
- Manage `clientsMap` (unordered_map<string, unique_ptr<Client>>)
- Insert requests into `queue` (set of unique_ptr<IServiceAction>)
- Persist iterator returned from insert into `ClientIdToQueueMap` to allow cancel-by-id
- Serve: pop `*begin(queue)`, call `execute()`, free memory
- CLI helpers: add, cancel, printq, printc, serve, exit
- JSON loader: populate clients and starting queue entries at startup

---

## Build and run (example)
Requirements:
- g++ or clang with C++17 support
- single-header `nlohmann::json` in the include path (or vendor it under third_party)

Compile:
```bash
g++ -std=c++17 BankQueueManager.cpp -o bankq
```

Run:
```bash
./bankq
# or run scripted demo
./bankq < demo-commands.txt
```

CLI commands (examples):
- `add <clientId> deposit <amount>`
- `add <clientId> withdraw <amount>`
- `add <clientId> transfer <amount> <targetClientId>`
- `add <clientId> check`
- `cancel <clientId>`
- `serve`
- `printq`
- `printc`
- `exit`

---

## Example usage and sample flows
- Prepopulate `clients.json` and `starting_queue.json`, then run `./bankq` to exercise realistic scenarios.
- Use `demo-commands.txt` to replay a scripted scenario that includes deposits, withdrawals, transfers, cancels and final state inspection.

---

## Contact / Attribution
Author: Matan Shemesh

License: MIT (see LICENSE file)

---
