#include "keyboard.hpp"

#include "helper.hpp"

void Keyboard::toggleKey(const Napi::CallbackInfo &info) {
  keyToggler(Helper::keyboardButtons.at(info[0].As<Napi::String>()), info[1].As<Napi::Boolean>());
}

void Keyboard::printChar(const Napi::CallbackInfo &info) {
  charPrinter(info[0].As<Napi::Number>().Int32Value());
};