#pragma once

#include <string>
#include <memory>
#include <iostream>
using namespace std;

//••••••••••••••••••ServiceError••••••••••••••••••
class ServiceError {
public:
    enum class Code {
        None,
        NotFound,
        WrongPassword,
        InsufficientFunds,
        AccountInactive,
        DestinationInactive,
        InvalidAmount,
        InvalidArguments,
        AlreadyExists,
        HasDependents,
        Timeout,
        NotLoggedIn,
        AlreadyLoggedIn,
        NotOwner,
        Custom
    };

    Code code;
    string detail;

    ServiceError();
    explicit ServiceError(Code c, string d = "");
    static ServiceError none();
};

//••••••••••••••••••Result<T>••••••••••••••••••
template<typename T>
class Result {
private:
    bool ok_;
    T value_;
    ServiceError error_;

    Result(bool ok, T value, ServiceError error)
        : ok_(ok), value_(value), error_(error) {}

public:
    static Result success(T value) {
        return Result(true, value, ServiceError::none());
    }

    static Result failure(ServiceError err) {
        return Result(false, T(), err);
    }

    bool isOk() const { return ok_; }
    const T& getValue() const { return value_; }
    const ServiceError& getError() const { return error_; }
};

class VoidResult {
private:
    bool ok_;
    ServiceError error_;

    VoidResult(bool ok, ServiceError error);

public:
    static VoidResult success();
    static VoidResult failure(ServiceError err);

    bool isOk() const;
    const ServiceError& getError() const;
};

//••••••••••••••••••ErrorReporter••••••••••••••••••
class ErrorReporter {
public:
    static void report(const ServiceError& err);

private:
    static string messageFor(const ServiceError& err);
};