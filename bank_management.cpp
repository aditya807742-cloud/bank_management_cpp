#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {
constexpr const char* DATA_FILE = "accounts.txt";
constexpr const char* TEMP_FILE = "accounts.tmp";
constexpr long long FIRST_ACCOUNT_NUMBER = 100001;

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

bool parsePositiveAmount(const std::string& input, long long& amountPaise) {
    const std::string value = trim(input);
    if (value.empty() || value.front() == '-') return false;

    const std::size_t dot = value.find('.');
    if (dot != std::string::npos && value.find('.', dot + 1) != std::string::npos) {
        return false;
    }

    const std::string rupeePart = dot == std::string::npos ? value : value.substr(0, dot);
    const std::string paisePart = dot == std::string::npos ? "" : value.substr(dot + 1);
    if (rupeePart.empty() || (dot != std::string::npos && paisePart.empty()) || paisePart.size() > 2) {
        return false;
    }
    if (!std::all_of(rupeePart.begin(), rupeePart.end(), [](unsigned char c) { return std::isdigit(c); }) ||
        !std::all_of(paisePart.begin(), paisePart.end(), [](unsigned char c) { return std::isdigit(c); })) {
        return false;
    }

    try {
        const long long rupees = std::stoll(rupeePart);
        const int paise = paisePart.empty() ? 0 :
                          (paisePart.size() == 1 ? (paisePart[0] - '0') * 10
                                                 : (paisePart[0] - '0') * 10 + (paisePart[1] - '0'));
        if (rupees > (std::numeric_limits<long long>::max() - paise) / 100) return false;
        amountPaise = rupees * 100 + paise;
        return amountPaise > 0;
    } catch (const std::exception&) {
        return false;
    }
}

std::string formatMoney(long long amountPaise) {
    std::ostringstream output;
    output << "Rs. " << amountPaise / 100 << '.' << std::setw(2) << std::setfill('0')
           << amountPaise % 100;
    return output.str();
}

long long readInteger(const std::string& prompt, long long minimum, long long maximum) {
    while (true) {
        std::cout << prompt;
        std::string line;
        if (!std::getline(std::cin, line)) return minimum;
        line = trim(line);
        try {
            std::size_t used = 0;
            const long long value = std::stoll(line, &used);
            if (used == line.size() && value >= minimum && value <= maximum) return value;
        } catch (const std::exception&) {
            // Show the same validation message for malformed and out-of-range input.
        }
        std::cout << "Invalid input. Enter a number between " << minimum << " and " << maximum << ".\n";
    }
}

std::string readName() {
    while (true) {
        std::cout << "Enter account holder name: ";
        std::string name;
        if (!std::getline(std::cin, name)) return "";
        name = trim(name);
        if (!name.empty() && name.size() <= 100) return name;
        std::cout << "Name cannot be empty and must be at most 100 characters.\n";
    }
}

long long readAmount() {
    while (true) {
        std::cout << "Enter amount (e.g. 1500 or 1500.50): ";
        std::string input;
        if (!std::getline(std::cin, input)) return 0;
        long long amount = 0;
        if (parsePositiveAmount(input, amount)) return amount;
        std::cout << "Invalid amount. Enter a positive amount with up to two decimal places.\n";
    }
}
} // namespace

class BankAccount {
private:
    long long accountNumber_;
    std::string holderName_;
    long long balancePaise_;

public:
    BankAccount(long long accountNumber, std::string holderName, long long balancePaise = 0)
        : accountNumber_(accountNumber), holderName_(std::move(holderName)), balancePaise_(balancePaise) {}

    long long getAccountNumber() const { return accountNumber_; }
    const std::string& getHolderName() const { return holderName_; }
    long long getBalancePaise() const { return balancePaise_; }

    bool deposit(long long amountPaise) {
        if (amountPaise <= 0 || balancePaise_ > std::numeric_limits<long long>::max() - amountPaise) {
            return false;
        }
        balancePaise_ += amountPaise;
        return true;
    }

    bool withdraw(long long amountPaise) {
        if (amountPaise <= 0 || amountPaise > balancePaise_) return false;
        balancePaise_ -= amountPaise;
        return true;
    }

    void setBalance(long long balancePaise) { balancePaise_ = balancePaise; }
};

class Bank {
private:
    std::vector<BankAccount> accounts_;
    long long nextAccountNumber_ = FIRST_ACCOUNT_NUMBER;

    BankAccount* findAccount(long long accountNumber) {
        const auto it = std::find_if(accounts_.begin(), accounts_.end(),
            [accountNumber](const BankAccount& account) {
                return account.getAccountNumber() == accountNumber;
            });
        return it == accounts_.end() ? nullptr : &(*it);
    }

    bool save() const {
        std::ofstream output(TEMP_FILE, std::ios::trunc);
        if (!output) {
            std::cerr << "Error: Could not create the temporary account data file.\n";
            return false;
        }

        for (const BankAccount& account : accounts_) {
            output << account.getAccountNumber() << ' '
                   << std::quoted(account.getHolderName()) << ' '
                   << account.getBalancePaise() << '\n';
        }
        output.flush();
        if (!output) {
            std::cerr << "Error: Could not write account data.\n";
            output.close();
            std::error_code ignored;
            std::filesystem::remove(TEMP_FILE, ignored);
            return false;
        }
        output.close();

        std::error_code error;
        std::filesystem::rename(TEMP_FILE, DATA_FILE, error);
        if (error) {
            std::cerr << "Error: Could not replace the account data file: " << error.message() << '\n';
            std::error_code ignored;
            std::filesystem::remove(TEMP_FILE, ignored);
            return false;
        }

        // Restrict the data file to the current user where the platform supports these permissions.
        std::filesystem::permissions(DATA_FILE,
            std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
            std::filesystem::perm_options::replace, error);
        return true;
    }

public:
    bool load() {
        std::ifstream input(DATA_FILE);
        if (!input) {
            if (std::filesystem::exists(DATA_FILE)) {
                std::cerr << "Error: Account data file exists but cannot be opened.\n";
                return false;
            }
            return true; // First run: no account file exists yet.
        }

        std::vector<BankAccount> loadedAccounts;
        long long largestAccountNumber = FIRST_ACCOUNT_NUMBER - 1;
        std::string line;
        std::size_t lineNumber = 0;
        while (std::getline(input, line)) {
            ++lineNumber;
            if (trim(line).empty()) continue;

            std::istringstream row(line);
            long long accountNumber = 0;
            long long balancePaise = 0;
            std::string holderName;
            std::string extra;
            if (!(row >> accountNumber >> std::quoted(holderName) >> balancePaise) || (row >> extra) ||
                accountNumber <= 0 || holderName.empty() || holderName.size() > 100 || balancePaise < 0) {
                std::cerr << "Error: Invalid data on line " << lineNumber << " of " << DATA_FILE << ".\n";
                return false;
            }

            const bool duplicate = std::any_of(loadedAccounts.begin(), loadedAccounts.end(),
                [accountNumber](const BankAccount& account) {
                    return account.getAccountNumber() == accountNumber;
                });
            if (duplicate) {
                std::cerr << "Error: Duplicate account number on line " << lineNumber << ".\n";
                return false;
            }

            loadedAccounts.emplace_back(accountNumber, holderName, balancePaise);
            largestAccountNumber = std::max(largestAccountNumber, accountNumber);
        }
        if (input.bad()) {
            std::cerr << "Error: A read error occurred while loading account data.\n";
            return false;
        }
        if (largestAccountNumber == std::numeric_limits<long long>::max()) {
            std::cerr << "Error: No further account numbers can be generated.\n";
            return false;
        }

        accounts_ = std::move(loadedAccounts);
        nextAccountNumber_ = std::max(FIRST_ACCOUNT_NUMBER, largestAccountNumber + 1);
        return true;
    }

    bool createAccount(const std::string& holderName) {
        if (nextAccountNumber_ == std::numeric_limits<long long>::max()) {
            std::cout << "Unable to create an account: account number limit reached.\n";
            return false;
        }
        accounts_.emplace_back(nextAccountNumber_, holderName, 0);
        if (!save()) {
            accounts_.pop_back();
            return false;
        }
        std::cout << "Account created successfully. Your account number is: "
                  << nextAccountNumber_ << '\n';
        ++nextAccountNumber_;
        return true;
    }

    void depositMoney(long long accountNumber, long long amountPaise) {
        BankAccount* account = findAccount(accountNumber);
        if (!account) {
            std::cout << "Account not found.\n";
            return;
        }
        const long long oldBalance = account->getBalancePaise();
        if (!account->deposit(amountPaise)) {
            std::cout << "Deposit could not be completed because the amount is invalid or too large.\n";
            return;
        }
        if (!save()) {
            account->setBalance(oldBalance);
            std::cout << "Deposit cancelled because the updated record could not be saved.\n";
            return;
        }
        std::cout << "Deposit successful. Updated balance: " << formatMoney(account->getBalancePaise()) << '\n';
    }

    void withdrawMoney(long long accountNumber, long long amountPaise) {
        BankAccount* account = findAccount(accountNumber);
        if (!account) {
            std::cout << "Account not found.\n";
            return;
        }
        const long long oldBalance = account->getBalancePaise();
        if (!account->withdraw(amountPaise)) {
            std::cout << "Withdrawal failed. Check the amount and ensure sufficient balance.\n";
            return;
        }
        if (!save()) {
            account->setBalance(oldBalance);
            std::cout << "Withdrawal cancelled because the updated record could not be saved.\n";
            return;
        }
        std::cout << "Withdrawal successful. Remaining balance: " << formatMoney(account->getBalancePaise()) << '\n';
    }

    void showBalance(long long accountNumber) const {
        const auto it = std::find_if(accounts_.begin(), accounts_.end(),
            [accountNumber](const BankAccount& account) {
                return account.getAccountNumber() == accountNumber;
            });
        if (it == accounts_.end()) {
            std::cout << "Account not found.\n";
            return;
        }
        std::cout << "\nAccount Number : " << it->getAccountNumber()
                  << "\nAccount Holder : " << it->getHolderName()
                  << "\nAvailable Balance: " << formatMoney(it->getBalancePaise()) << "\n";
    }

    void displayAllAccounts() const {
        if (accounts_.empty()) {
            std::cout << "No accounts are currently registered.\n";
            return;
        }
        std::cout << "\n---------------------- ACCOUNT LIST ----------------------\n";
        std::cout << std::left << std::setw(18) << "Account Number"
                  << std::setw(28) << "Account Holder" << "Balance\n";
        std::cout << std::string(65, '-') << '\n';
        for (const BankAccount& account : accounts_) {
            std::cout << std::left << std::setw(18) << account.getAccountNumber()
                      << std::setw(28) << account.getHolderName()
                      << formatMoney(account.getBalancePaise()) << '\n';
        }
    }
};

int main() {
    Bank bank;
    if (!bank.load()) {
        std::cerr << "The application cannot safely continue until the account data is corrected.\n";
        return 1;
    }

    std::cout << "==============================================\n"
              << "          BANK MANAGEMENT APPLICATION        \n"
              << "==============================================\n";

    bool running = true;
    while (running) {
        std::cout << "\n--------------- MAIN MENU -------------------\n"
                  << "1. Create New Account\n"
                  << "2. Deposit Money\n"
                  << "3. Withdraw Money\n"
                  << "4. Check Account Balance\n"
                  << "5. Display All Accounts\n"
                  << "6. Exit\n"
                  << "----------------------------------------------\n";

        const long long choice = readInteger("Enter your choice (1-6): ", 1, 6);
        switch (choice) {
            case 1:
                bank.createAccount(readName());
                break;
            case 2: {
                const long long accountNumber = readInteger("Enter account number: ", 1,
                    std::numeric_limits<long long>::max());
                bank.depositMoney(accountNumber, readAmount());
                break;
            }
            case 3: {
                const long long accountNumber = readInteger("Enter account number: ", 1,
                    std::numeric_limits<long long>::max());
                bank.withdrawMoney(accountNumber, readAmount());
                break;
            }
            case 4: {
                const long long accountNumber = readInteger("Enter account number: ", 1,
                    std::numeric_limits<long long>::max());
                bank.showBalance(accountNumber);
                break;
            }
            case 5:
                bank.displayAllAccounts();
                break;
            case 6:
                running = false;
                std::cout << "Thank you for using the Bank Management Application.\n";
                break;
            default:
                break;
        }
    }
    return 0;
}
