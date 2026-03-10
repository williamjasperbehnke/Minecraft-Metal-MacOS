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
  const Slot& carriedSlot() const;
  bool hasCarriedSlot() const;
  int selectedTile() const;
  bool isValidSlotIndex(int index) const;

  void leftClickSlot(int slotIndex);
  void rightClickSlot(int slotIndex);
  void leftClickOutside();
  void rightClickOutside();
  void shiftLeftClickSlot(int slotIndex);
  void middleClickSlot(int slotIndex, bool creativeMode);
  void doubleClickCollect(int tile);
  void hotbarSwapSlot(int slotIndex, int hotbarIndex);
  void dropFromSlot(int slotIndex, bool dropStack);
  int addItem(int tile, int count);
  void beginDragSplit();
  void dragSplitAddSlot(int slotIndex);
  void endDragSplit();
  bool isDragSplitActive() const { return dragSplitActive_; }

private:
  static const Slot kEmptySlot;
  static constexpr int kMaxStackSize = 64;

  bool open_ = false;
  int selectedHotbar_ = 0;
  std::array<Slot, kTotalSlots> slots_{};
  Slot carried_{};

  static bool isEmptySlot(const Slot& slot);
  static bool canStack(const Slot& a, const Slot& b);
  static void normalizeSlot(Slot& slot);
  static int stackSpace(const Slot& slot);
  void quickMoveToRange(int fromSlotIndex, int begin, int end);
  bool canAcceptDragSplit(const Slot& slot) const;

  bool dragSplitActive_ = false;
  std::array<bool, kTotalSlots> dragSplitVisited_{};
};

}  // namespace mc
