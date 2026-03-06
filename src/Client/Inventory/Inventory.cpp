#include "Client/Inventory/Inventory.h"

#include <algorithm>

#include "World/Tile/Tile.h"

namespace mc {

const Inventory::Slot Inventory::kEmptySlot{};

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
  if (index < 0 || index >= kTotalSlots) {
    return kEmptySlot;
  }
  return slots_[static_cast<std::size_t>(index)];
}

int Inventory::selectedTile() const {
  const Slot& selected = hotbarSlot(selectedHotbar_);
  if (selected.count <= 0 || selected.tile <= 0) {
    return static_cast<int>(TileId::Grass);
  }
  return selected.tile;
}

}  // namespace mc
