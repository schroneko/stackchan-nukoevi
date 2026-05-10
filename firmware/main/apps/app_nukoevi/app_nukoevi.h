#pragma once
#include <mooncake.h>

class AppNukoevi : public mooncake::AppAbility {
public:
    AppNukoevi();

    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;
};
