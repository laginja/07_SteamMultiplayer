// Fill out your copyright notice in the Description page of Project Settings.

#include "PuzzlePlatformsGameInstance.h"

#include "Engine/Engine.h"
#include "UObject/ConstructorHelpers.h"
#include "Blueprint/UserWidget.h"
#include "OnlineSessionSettings.h"

#include "MenuSystem/MainMenu.h"
#include "MenuSystem/MenuWidget.h"
#include "PuzzlePlatformsGameMode.h"

const static FName SESSION_NAME = TEXT("My Session Game");

UPuzzlePlatformsGameInstance::UPuzzlePlatformsGameInstance(const FObjectInitializer & ObjectInitializer)
{
	// A way to get ahold of a BP class
	ConstructorHelpers::FClassFinder<UUserWidget> MenuBPClass(TEXT("/Game/MenuSystem/WBP_MainMenu"));
	if (!ensure(MenuBPClass.Class != nullptr)) return;

	MenuClass = MenuBPClass.Class;

	// A way to get ahold of a BP class
	ConstructorHelpers::FClassFinder<UUserWidget> InGameMenuBPClass(TEXT("/Game/MenuSystem/WBP_InGameMenu"));
	if (!ensure(InGameMenuBPClass.Class != nullptr)) return;

	InGameMenuClass = InGameMenuBPClass.Class;
}

void UPuzzlePlatformsGameInstance::Init()
{
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (Subsystem != nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Found subsystem %s"), *Subsystem->GetSubsystemName().ToString());
		SessionInterface = Subsystem->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			// moraju se pozvati delegati jer se radi o asinkronom dogadaju; npr. CreateSession samo pokrece proces kreiranja sesije dok se u pozadini nastavlja
			// vrtiti program sto omogucava igranje igre u isto vrijeme dok se sesija ne kreira.
			// MulticastDelegae (nije DynamicDelegate) ->nema potrebe za koristenjem UFUNCTIONS kao kod Dynamic
			SessionInterface->OnCreateSessionCompleteDelegates.AddUObject(this, &UPuzzlePlatformsGameInstance::OnCreateSessionComplete);
			SessionInterface->OnDestroySessionCompleteDelegates.AddUObject(this, &UPuzzlePlatformsGameInstance::OnDestroySessionComplete);
			SessionInterface->OnFindSessionsCompleteDelegates.AddUObject(this, &UPuzzlePlatformsGameInstance::OnFindSessionsComplete);
			SessionInterface->OnJoinSessionCompleteDelegates.AddUObject(this, &UPuzzlePlatformsGameInstance::OnJoinSessionComplete);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Found no subsystem"));
	}
}

void UPuzzlePlatformsGameInstance::LoadMenuWidget()
{
	if (!ensure(MenuClass != nullptr)) return;

	// This creates a Menu object
	Menu = CreateWidget<UMainMenu>(this, MenuClass);
	if (!ensure(Menu != nullptr)) return;

	Menu->Setup();
	Menu->SetMenuInterface(this);
}

void UPuzzlePlatformsGameInstance::InGameLoadMenu()
{
	if (!ensure(InGameMenuClass != nullptr)) return;

	// This creates a Menu object
	UMenuWidget* Menu = CreateWidget<UMenuWidget>(this, InGameMenuClass);
	if (!ensure(Menu != nullptr)) return;

	Menu->Setup();
	Menu->SetMenuInterface(this);
}

void UPuzzlePlatformsGameInstance::Host()
{
	if (SessionInterface.IsValid())
	{	

		auto ExistingSession = SessionInterface->GetNamedSession(SESSION_NAME);
		if (ExistingSession != nullptr)
		{
			SessionInterface->DestroySession(SESSION_NAME);
		}
		else
		{
			CreateSession();
		}
	}
}

void UPuzzlePlatformsGameInstance::OnCreateSessionComplete(FName SessionName, bool Success)
{
	if (!Success)
	{
		UE_LOG(LogTemp, Warning, TEXT("Could not create session"));
		return;
	}
	if (Menu != nullptr)
	{
		Menu->Teardown();
	}

	UEngine* Engine = GetEngine();
	if (!ensure(Engine != nullptr)) return;

	Engine->AddOnScreenDebugMessage(0, 5.f, FColor::Red, TEXT("Hosting"));

	UWorld* World = GetWorld();
	if (!ensure(World != nullptr)) return;

	World->ServerTravel("/Game/ThirdPersonCPP/Maps/ThirdPersonExampleMap?listen");
}

void UPuzzlePlatformsGameInstance::OnDestroySessionComplete(FName SessionName, bool Success)
{
	if (Success)
	{
		CreateSession();
	}
}

void UPuzzlePlatformsGameInstance::RefreshServerList()
{
	SessionSearch = MakeShareable(new FOnlineSessionSearch());
	if (SessionSearch.IsValid())
	{
		SessionSearch->bIsLanQuery = true;
		UE_LOG(LogTemp, Warning, TEXT("Searching for sessions"));
		SessionInterface->FindSessions(0, SessionSearch.ToSharedRef());
	}
}


void UPuzzlePlatformsGameInstance::OnFindSessionsComplete(bool Success)
{
	if (Success && SessionSearch.IsValid() && Menu != nullptr)
	{
		auto SearchResults = SessionSearch->SearchResults;
		UE_LOG(LogTemp, Warning, TEXT("Finished Find Session"));

		TArray<FString> ServerNames;
		for (FOnlineSessionSearchResult& SearchResult : SearchResults)
		{
			FString SessionID = SearchResult.GetSessionIdStr();
			UE_LOG(LogTemp, Warning, TEXT("Session ID: %s"), *SessionID);
			ServerNames.Add(SessionID);
		}

		Menu->SetServerList(ServerNames);
	}
}

void UPuzzlePlatformsGameInstance::CreateSession()
{
	if (SessionInterface.IsValid())
	{
		FOnlineSessionSettings SessionSettings;
		// ove postavke trebaju biti inace nece pronaci niti jednu sesiju
		SessionSettings.bIsLANMatch = true;
		SessionSettings.NumPublicConnections = 2;
		SessionSettings.bShouldAdvertise = true;

		SessionInterface->CreateSession(0, SESSION_NAME, SessionSettings);
	}
}


void UPuzzlePlatformsGameInstance::Join(uint32 Index)
{
	if (!SessionInterface.IsValid()) return;
	if (!SessionSearch.IsValid()) return;

	if (Menu != nullptr)
	{
		Menu->Teardown();
	}

	SessionInterface->JoinSession(0, SESSION_NAME, SessionSearch->SearchResults[Index]);
}

void UPuzzlePlatformsGameInstance::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (!SessionInterface.IsValid()) return;

	FString Address;		// OUT param
	if (!SessionInterface->GetResolvedConnectString(SessionName, Address))		// "za ovaj SessionName daj mi njegovu IP adresu"
	{
		UE_LOG(LogTemp, Warning, TEXT("Could not get connect string"));
		return;
	}

	UEngine* Engine = GetEngine();
	if (!ensure(Engine != nullptr)) return;

	Engine->AddOnScreenDebugMessage(0, 5.f, FColor::Red, FString::Printf(TEXT("Joining %s"), *Address));

	APlayerController* PlayerController = GetFirstLocalPlayerController();
	if (!ensure(PlayerController != nullptr)) return;

	PlayerController->ClientTravel(*Address, ETravelType::TRAVEL_Absolute);
}

void UPuzzlePlatformsGameInstance::LoadMainMenu()
{
	// Working solution. If it's a server, when quitting all clients get moved to the Deafult Map(MainMenu).
	// If it's a client then only that client get's moved to the Default Map
	UWorld* World = GetWorld();
	if (!ensure(World != nullptr)) return;

	if (World->IsServer())
	{
		APuzzlePlatformsGameMode* GameMode = World->GetAuthGameMode<APuzzlePlatformsGameMode>();
		if (!ensure(GameMode != nullptr)) return;
		
		GameMode->ReturnToMainMenuHost();
	}
	else
	{
		APlayerController* PlayerController = GetFirstLocalPlayerController();
		if (!ensure(PlayerController != nullptr)) return;

		PlayerController->ClientReturnToMainMenu("Back to main menu");
	}
}



