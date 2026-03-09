#include "Client/Inventory/Inventory.h"

#include <algorithm>

#include "World/Tile/Tile.h"

namespace mc {

const Inventory::Slot Inventory::kEmptySlot{};

bool Inventory::isEmptySlot(const Slot& slot) {
  return slot.tile <= 0 || slot.count <= 0;
}

bool Inventory::canStack(const Slot& a, const Slot& b) {
  return !isEmptySlot(a) && !isEmptySlot(b) && a.tile == b.tile;
}

void Inventory::normalizeSlot(Slot& slot) {
  if (slot.tile <= 0 || slot.count <= 0) {
    slot = {};
  }
}

int Inventory::stackSpace(const Slot& slot) {
  if (isEmptySlot(slot)) {
    return kMaxStackSize;
  }
  return std::max(0, kMaxStackSize - slot.count);
}

Inventory::Inventory() {
  // PS3-inspired starter hotbar ordering for creative-like quick testing.
  slots_[0] = Slot{static_cast<int>(TileId::Grass), 64};
  slots_[1] = Slot{static_cast<int>(TileId::Dirt), 64};
  slots_[2] = Slot{static_cast<int>(TileId::Stone), 64};
  slots_[3] = Slot{static_cast<int>(TileId::IronOre), 64};
  slots_[4] = Slot{static_cast<int>(TileId::Wood), 64};
  slots_[5] = Slot{static_cast<int>(TileId::Planks), 64};
  slots_[6] = Slot{static_cast<int>(TileId::Sand), 64};
  slots_[7] = Slot{static_cast<int>(TileId::Glass), 64};
  slots_[8] = Slot{static_cast<int>(TileId::Water), 64};

  // Main inventory blocks used by terrain/biomes.
  slots_[9] = Slot{static_cast<int>(TileId::Leaves), 64};
  slots_[10] = Slot{static_cast<int>(TileId::SpruceLeaves), 64};
  slots_[11] = Slot{static_cast<int>(TileId::BirchLeaves), 64};
  slots_[12] = Slot{static_cast<int>(TileId::SpruceWood), 64};
  slots_[13] = Slot{static_cast<int>(TileId::BirchWood), 64};
  slots_[14] = Slot{static_cast<int>(TileId::Sandstone), 64};
  slots_[15] = Slot{static_cast<int>(TileId::Gravel), 64};
  slots_[16] = Slot{static_cast<int>(TileId::Clay), 64};
  slots_[17] = Slot{static_cast<int>(TileId::Snow), 64};
  slots_[18] = Slot{static_cast<int>(TileId::Ice), 64};
  slots_[19] = Slot{static_cast<int>(TileId::Cactus), 64};
  slots_[20] = Slot{static_cast<int>(TileId::TallGrass), 64};
  slots_[21] = Slot{static_cast<int>(TileId::Fern), 64};
  slots_[22] = Slot{static_cast<int>(TileId::FlowerYellow), 64};
  slots_[23] = Slot{static_cast<int>(TileId::FlowerRed), 64};
  slots_[24] = Slot{static_cast<int>(TileId::MushroomBrown), 64};
  slots_[25] = Slot{static_cast<int>(TileId::MushroomRed), 64};
  slots_[26] = Slot{static_cast<int>(TileId::SugarCane), 64};
  slots_[27] = Slot{static_cast<int>(TileId::CoalOre), 64};
  slots_[28] = Slot{static_cast<int>(TileId::Cobblestone), 64};
  slots_[29] = Slot{static_cast<int>(TileId::GoldOre), 64};
  slots_[30] = Slot{static_cast<int>(TileId::DiamondOre), 64};
}

void Inventory::selectHotbarIndex(int index) {
  selectedHotbar_ = std::clamp(index, 0, kHotbarSize - 1);
}

void Inventory::cycleHotbar(int delta) {
  const int wrapped = (selectedHotbar_ + delta) % kHotbarSize;
  selectedHotbar_ = (wrapped < 0) ? (wrapped + kHotbarSize) : wrapped;
}

const Inventory::Slot& Inventory::hotbarSlot(int index) const {
  if (index < 0 || index >= kHotbarSize) {
    return kEmptySlot;
  }
  return slots_[static_cast<std::size_t>(index)];
}

const Inventory::Slot& Inventory::slot(int index) const {
  if (!isValidSlotIndex(index)) {
    return kEmptySlot;
  }
  return slots_[static_cast<std::size_t>(index)];
}

const Inventory::Slot& Inventory::carriedSlot() const {
  return carried_;
}

bool Inventory::hasCarriedSlot() const {
  return !isEmptySlot(carried_);
}

bool Inventory::isValidSlotIndex(int index) const {
  return index >= 0 && index < kTotalSlots;
}

int Inventory::selectedTile() const {
  const Slot& selected = hotbarSlot(selectedHotbar_);
  if (selected.count <= 0 || selected.tile <= 0) {
    return static_cast<int>(TileId::Grass);
  }
  return selected.tile;
}

void Inventory::leftClickSlot(int slotIndex) {
  if (!isValidSlotIndex(slotIndex)) {
    return;
  }
  Slot& target = slots_[static_cast<std::size_t>(slotIndex)];
  normalizeSlot(target);
  normalizeSlot(carried_);

  if (isEmptySlot(carried_)) {
    if (isEmptySlot(target)) {
      return;
    }
    carried_ = target;
    target = {};
    return;
  }

  if (isEmptySlot(target)) {
    target = carried_;
    carried_ = {};
    return;
  }

  if (canStack(carried_, target)) {
    const int moved = std::min(stackSpace(target), carried_.count);
    target.count += moved;
    carried_.count -= moved;
    normalizeSlot(carried_);
    return;
  }

  std::swap(target, carried_);
}

void Inventory::rightClickSlot(int slotIndex) {
  if (!isValidSlotIndex(slotIndex)) {
    return;
  }
  Slot& target = slots_[static_cast<std::size_t>(slotIndex)];
  normalizeSlot(target);
  normalizeSlot(carried_);

  if (isEmptySlot(carried_)) {
    if (isEmptySlot(target)) {
      return;
    }
    const int taken = (target.count + 1) / 2;
    carried_ = {target.tile, taken};
    target.count -= taken;
    normalizeSlot(target);
    return;
  }

  if (isEmptySlot(target)) {
    target = {carried_.tile, 1};
    carried_.count -= 1;
    normalizeSlot(carried_);
    return;
  }

  if (canStack(carried_, target) && target.count < kMaxStackSize) {
    target.count += 1;
    carried_.count -= 1;
    normalizeSlot(carried_);
    return;
  }

  std::swap(target, carried_);
}

void Inventory::leftClickOutside() {
  carried_ = {};
}

void Inventory::rightClickOutside() {
  normalizeSlot(carried_);
  if (isEmptySlot(carried_)) {
    return;
  }
  carried_.count -= 1;
  normalizeSlot(carried_);
}

void Inventory::quickMoveToRange(int fromSlotIndex, int begin, int end) {
  if (!isValidSlotIndex(fromSlotIndex) || begin < 0 || end > kTotalSlots || begin >= end) {
    return;
  }

  Slot moving = slots_[static_cast<std::size_t>(fromSlotIndex)];
  normalizeSlot(moving);
  if (isEmptySlot(moving)) {
    return;
  }
  slots_[static_cast<std::size_t>(fromSlotIndex)] = {};

  for (int i = begin; i < end && !isEmptySlot(moving); ++i) {
    Slot& dst = slots_[static_cast<std::size_t>(i)];
    normalizeSlot(dst);
    if (!canStack(moving, dst) || dst.count >= kMaxStackSize) {
      continue;
    }
    const int moved = std::min(stackSpace(dst), moving.count);
    dst.count += moved;
    moving.count -= moved;
    normalizeSlot(moving);
  }

  for (int i = begin; i < end && !isEmptySlot(moving); ++i) {
    Slot& dst = slots_[static_cast<std::size_t>(i)];
    normalizeSlot(dst);
    if (!isEmptySlot(dst)) {
      continue;
    }
    dst = moving;
    moving = {};
  }

  slots_[static_cast<std::size_t>(fromSlotIndex)] = moving;
}

void Inventory::shiftLeftClickSlot(int slotIndex) {
  if (!isValidSlotIndex(slotIndex)) {
    return;
  }
  if (slotIndex < kHotbarSize) {
    quickMoveToRange(slotIndex, kHotbarSize, kTotalSlots);
  } else {
    quickMoveToRange(slotIndex, 0, kHotbarSize);
  }
}

void Inventory::middleClickSlot(int slotIndex, bool creativeMode) {
  if (!creativeMode || !isValidSlotIndex(slotIndex)) {
    return;
  }
  const Slot& target = slots_[static_cast<std::size_t>(slotIndex)];
  if (isEmptySlot(target)) {
    return;
  }
  carried_ = {target.tile, kMaxStackSize};
}

void Inventory::doubleClickCollect(int tile) {
  normalizeSlot(carried_);
  if (tile <= 0) {
    return;
  }
  if (isEmptySlot(carried_)) {
    carried_ = {tile, 0};
  }
  if (carried_.tile != tile || carried_.count >= kMaxStackSize) {
    return;
  }

  int needed = kMaxStackSize - carried_.count;
  for (Slot& slotRef : slots_) {
    normalizeSlot(slotRef);
    if (needed <= 0 || slotRef.tile != tile || slotRef.count <= 0) {
      continue;
    }
    const int moved = std::min(needed, slotRef.count);
    carried_.count += moved;
    slotRef.count -= moved;
    needed -= moved;
    normalizeSlot(slotRef);
  }
  normalizeSlot(carried_);
}

void Inventory::hotbarSwapSlot(int slotIndex, int hotbarIndex) {
  if (!isValidSlotIndex(slotIndex) || hotbarIndex < 0 || hotbarIndex >= kHotbarSize) {
    return;
  }
  if (slotIndex == hotbarIndex) {
    return;
  }
  std::swap(slots_[static_cast<std::size_t>(slotIndex)], slots_[static_cast<std::size_t>(hotbarIndex)]);
}

void Inventory::dropFromSlot(int slotIndex, bool dropStack) {
  if (!isValidSlotIndex(slotIndex)) {
    return;
  }
  Slot& target = slots_[static_cast<std::size_t>(slotIndex)];
  normalizeSlot(target);
  if (isEmptySlot(target)) {
    return;
  }
  if (dropStack) {
    target = {};
    return;
  }
  target.count -= 1;
  normalizeSlot(target);
}

bool Inventory::canAcceptDragSplit(const Slot& slot) const {
  if (isEmptySlot(carried_)) {
    return false;
  }
  if (isEmptySlot(slot)) {
    return true;
  }
  return slot.tile == carried_.tile && slot.count < kMaxStackSize;
}

void Inventory::beginDragSplit() {
  dragSplitActive_ = hasCarriedSlot();
  dragSplitVisited_.fill(false);
}

void Inventory::dragSplitAddSlot(int slotIndex) {
  if (!dragSplitActive_ || !isValidSlotIndex(slotIndex)) {
    return;
  }
  const Slot& target = slots_[static_cast<std::size_t>(slotIndex)];
  if (!canAcceptDragSplit(target)) {
    return;
  }
  dragSplitVisited_[static_cast<std::size_t>(slotIndex)] = true;
}

void Inventory::endDragSplit() {
  if (!dragSplitActive_) {
    return;
  }
  dragSplitActive_ = false;
  normalizeSlot(carried_);
  if (isEmptySlot(carried_)) {
    dragSplitVisited_.fill(false);
    return;
  }

  int selectedCount = 0;
  for (bool selected : dragSplitVisited_) {
    if (selected) {
      ++selectedCount;
    }
  }
  if (selectedCount <= 0) {
    dragSplitVisited_.fill(false);
    return;
  }

  int remaining = carried_.count;
  int slotsLeft = selectedCount;
  for (int i = 0; i < kTotalSlots && remaining > 0; ++i) {
    if (!dragSplitVisited_[static_cast<std::size_t>(i)]) {
      continue;
    }
    Slot& dst = slots_[static_cast<std::size_t>(i)];
    normalizeSlot(dst);
    if (!canAcceptDragSplit(dst)) {
      --slotsLeft;
      continue;
    }
    const int ideal = std::max(1, remaining / std::max(1, slotsLeft));
    const int moved = std::min(ideal, std::min(stackSpace(dst), remaining));
    if (moved <= 0) {
      --slotsLeft;
      continue;
    }
    if (isEmptySlot(dst)) {
      dst.tile = carried_.tile;
      dst.count = 0;
    }
    dst.count += moved;
    remaining -= moved;
    --slotsLeft;
  }

  carried_.count = remaining;
  normalizeSlot(carried_);
  dragSplitVisited_.fill(false);
}

}  // namespace mc
