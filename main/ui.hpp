#pragma once

#include "sen55.hpp"

class Ui {
public:
    Ui();
    ~Ui();

    Ui(const Ui&) = delete;
    Ui& operator=(const Ui&) = delete;
    Ui(Ui&&) = delete;
    Ui& operator=(Ui&&) = delete;

    void UpdateMeasurements(const Sen55::Measurement& data);
    void SetStatus(const char* text);

private:
    struct Impl;
    Impl* impl_;
};
