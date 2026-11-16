#pragma once
#include "input.h"

class Page {
 public:
  virtual ~Page() {}
  virtual const char *title() const = 0;
  virtual const char *hint() const = 0;
  virtual void enter() = 0;
  virtual void update() = 0;
  virtual void onInput(InputEvent) {}
  virtual void onNavigate(InputEvent) {}
  virtual void leave() {}
  virtual bool capturesNav() const { return false; }

  bool takeFullRedraw() {
    bool r = _redraw;
    _redraw = false;
    return r;
  }

 protected:
  bool _redraw = false;
};

class PageAuto : public Page {
 public:
  const char *title() const override;
  const char *hint() const override;
  void enter() override;
  void update() override;
  void onInput(InputEvent e) override;
};

class PageWifi : public Page {
 public:
  const char *title() const override;
  const char *hint() const override;
  void enter() override;
  void update() override;
  void onInput(InputEvent e) override;
};

class PageImu : public Page {
 public:
  const char *title() const override;
  const char *hint() const override;
  void enter() override;
  void update() override;
  void onInput(InputEvent e) override;
};

class PageAudio : public Page {
 public:
  const char *title() const override;
  const char *hint() const override;
  void enter() override;
  void update() override;
  void onInput(InputEvent e) override;
  void leave() override;
};

class PageLight : public Page {
 public:
  const char *title() const override;
  const char *hint() const override;
  void enter() override;
  void update() override;
  void onInput(InputEvent e) override;
  void onNavigate(InputEvent e) override;
  void leave() override;
  bool capturesNav() const override;
};

class PageSystem : public Page {
 public:
  const char *title() const override;
  const char *hint() const override;
  void enter() override;
  void update() override;
  void onInput(InputEvent e) override;
};
