#include "../include/result.hpp"
using namespace std;

//••••••••••••••••••ServiceError••••••••••••••••••
ServiceError::ServiceError() : code(Code::None), detail("") {}

ServiceError::ServiceError(Code c, string d) : code(c), detail(d) {}

ServiceError ServiceError::none() {
    return ServiceError(Code::None);
}

//••••••••••••••••••Result<T>••••••••••••••••••
VoidResult::VoidResult(bool ok, ServiceError error) : ok_(ok), error_(error) {}

VoidResult VoidResult::success() {
    return VoidResult(true, ServiceError::none());
}

VoidResult VoidResult::failure(ServiceError err) {
    return VoidResult(false, err);
}

bool VoidResult::isOk() const {
    return ok_;
}

const ServiceError& VoidResult::getError() const {
    return error_;
}

//••••••••••••••••••ErrorReporter••••••••••••••••••
void ErrorReporter::report(const ServiceError& err) {
    string msg = messageFor(err);
    if (msg.rfind("Error: ", 0) == 0) {
        cout << msg << endl;
    } else {
        cout << "Error: " << msg << endl;
    }
}

string ErrorReporter::messageFor(const ServiceError& err) {
    if (!err.detail.empty()) return err.detail;

    switch (err.code) {
        case ServiceError::Code::NotFound:          return "Not found.";
        case ServiceError::Code::WrongPassword:     return "Wrong password.";
        case ServiceError::Code::InsufficientFunds: return "Insufficient funds.";
        case ServiceError::Code::AccountInactive:   return "Account is inactive.";
        case ServiceError::Code::DestinationInactive: return "Destination account is inactive.";
        case ServiceError::Code::InvalidAmount:     return "Amount must be positive.";
        case ServiceError::Code::InvalidArguments:  return "Invalid arguments.";
        case ServiceError::Code::AlreadyExists:     return "Already exists.";
        case ServiceError::Code::HasDependents:     return "Has dependent records.";
        case ServiceError::Code::Timeout:           return "Timed out.";
        case ServiceError::Code::NotLoggedIn:       return "No user logged in.";
        case ServiceError::Code::AlreadyLoggedIn:   return "User already logged in.";
        case ServiceError::Code::NotOwner:          return "Account does not belong to user.";
        case ServiceError::Code::Custom:            return err.detail.empty() ? "Custom error." : err.detail;
        default:                                    return "Unknown error.";
    }
}