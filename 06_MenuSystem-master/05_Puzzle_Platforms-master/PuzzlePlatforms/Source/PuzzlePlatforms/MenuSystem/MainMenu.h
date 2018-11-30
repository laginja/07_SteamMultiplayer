// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MenuWidget.h"
#include "MainMenu.generated.h"

//USTRUCT() mora biti ako se struktura koristi u unrealu
USTRUCT()
struct FServerData
{
	GENERATED_BODY()

	FString Name;
	uint16 CurrentPlayers;
	uint16 MaxPlayers;
	FString HostUsername;
};

/**
 * 
 */
UCLASS()
class PUZZLEPLATFORMS_API UMainMenu : public UMenuWidget
{
	GENERATED_BODY()

public:
	UMainMenu(const FObjectInitializer & ObjectInitializer);

	void SetServerList(TArray<FServerData> ServerNames);
	void SetWarningMessage(FString WarningMessage);
	void SelectIndex(uint32 Index);

protected:
	virtual bool Initialize() override;

private:
	TSubclassOf<class UUserWidget> ServerRowClass;

	UPROPERTY(meta = (BindWidget))
	class UButton* HostButton;

	UPROPERTY(meta = (BindWidget))
	class UButton* JoinButton;

	UPROPERTY(meta = (BindWidget))
	class UButton* QuitGameButton;

	UPROPERTY(meta = (BindWidget))
	class UButton* BackHostMenuButton;

	UPROPERTY(meta = (BindWidget))
	class UButton* ConfirmHostMenuButton;

	UPROPERTY(meta = (BindWidget))
	class UButton* BackJoinMenuButton;

	UPROPERTY(meta = (BindWidget))
	class UButton* ConfirmJoinMenuButton;

	UPROPERTY(meta = (BindWidget))
	class UWidgetSwitcher* MenuSwitcher;

	UPROPERTY(meta = (BindWidget))
	class UWidget* HostMenu;

	UPROPERTY(meta = (BindWidget))
	class UWidget* JoinMenu;

	UPROPERTY(meta = (BindWidget))
	class UWidget* MainMenu;

	UPROPERTY(meta = (BindWidget))
	class UPanelWidget* ServerList;

	UPROPERTY(meta = (BindWidget))
	class UEditableTextBox* GameName;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* WarningText;

	UFUNCTION()
	void HostServer();

	UFUNCTION()
	void JoinServer();

	UFUNCTION()
	void QuitGame();

	UFUNCTION()
	void OpenHostMenu();

	UFUNCTION()
	void OpenJoinMenu();

	UFUNCTION()
	void OpenMainMenu();

	TOptional<uint32> SelectedIndex;

	void UpdateChildren();
};
