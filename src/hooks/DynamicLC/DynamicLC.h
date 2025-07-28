class DynamicLC {

public:
    static void Install();
protected:
    static void InitLeveledItems(RE::InventoryChanges* inv);

private:

    DynamicLC() = delete;
    DynamicLC(const DynamicLC&) = delete;
    DynamicLC(DynamicLC&&) = delete;
    ~DynamicLC() = delete;

    DynamicLC& operator=(const DynamicLC&) = delete;
    DynamicLC& operator=(DynamicLC&&) = delete;

    inline static REL::Relocation<decltype(InitLeveledItems)> _InitLeveledItems;
};