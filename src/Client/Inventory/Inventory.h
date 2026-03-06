#pragma once

#include <array>
#include <cstddef>

namespace mc {

class Inventory {
public:
  struct Slot {
    int tile = 0;
    int count = 0;
  };

  static constexpr int kHotbarSize = 9;
  static constexpr int kMainRows = 3;
  static constexpr int kMainCols = 9;
  static constexpr int kMainSize = kMainRows * kMainCols;
  static constexpr int kTotalSlots = kHotbarSize + kMainSize;

  Inventory();

  bool isOpen() const { return open_; }
  void setOpen(bool open) { open_ = open; }
  void toggleOpen() { open_ = !open_; }

  int selectedHotbarIndex() const { return selectedHotbar_; }
  void selectHotbarIndex(int index);
  void cycleHotbar(int delta);

  const Slot& hotbarSlot(int index) const;
  const Slot& slot(int index) const;
  int selectedTile() const;

private:
  static const Slot kEmptySlot;

  bool open_ = false;
  int selectedHotbar_ = 0;
  std::array<Slot, kTotalSlots> slots_{};
};

}  // namespace mc
