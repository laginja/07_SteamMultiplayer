// Fill out your copyright notice in the Description page of Project Settings.

#include "PuzzlePlatformsGameInstance.h"

#include "Engine/Engine.h"
#include "UObject/ConstructorHelpers.h"
#include "Blueprint/UserWidget.h"
#include "OnlineSessionSettings.h"

#include "MenuSystem/MainMenu.h"
#include "MenuSystem/MenuWidget.h"
#include "PuzzlePlatformsGameMode.h"

const static FName SESSION_NAME = NAME_GameSession;
const static FName SERVER_NAME_SETTINGS_KEY = TEXT("ServerName");

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
			// MulticastDelegate (nije DynamicDelegate) ->nema potrebe za koristenjem UFUNCTIONS kao kod Dynamic
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

void UPuzzlePlatformsGameInstance::Host(FString ServerName)
{
	DesiredServerName = ServerName;
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

void UPuzzlePlatformsGameInstance::CreateSession()
{
	if (SessionInterface.IsValid())
	{
		// ove postavke trebaju biti inace nece pronaci niti jednu sesiju
		FOnlineSessionSettings SessionSettings;
		if (IOnlineSubsystem::Get()->GetSubsystemName() == "NULL")
		{
			SessionSettings.bIsLANMatch = true;
		}
		else
		{
			SessionSettings.bIsLANMatch = false;
		}
		SessionSettings.NumPublicConnections = 5;
		SessionSettings.bShouldAdvertise = true;
		// ako se ne spajamo preko LAN-a onda ovo mora biti true
		SessionSettings.bUsesPresence = true;
		// ovako mozemo postaviti custom settings. Key-value.
		// Kod pronalaska sesija, sada ce i taj podatak biti dostupan za koristenje
		SessionSettings.Set(SERVER_NAME_SETTINGS_KEY, DesiredServerName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
		SessionInterface->CreateSession(0, SESSION_NAME, SessionSettings);
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

	// napravi server travel u lobby
	World->ServerTravel("/Game/PuzzlePlatforms/Maps/Lobby?listen");
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
		//SessionSearch->bIsLanQuery = true;
		// posto shareamo APPID sa svima koji koriste besplatnu verziju, po defaultu se vraca jedan rezultat i taj rezultat vrlo vjerojatno nije nas. Zato se ovo postavi na velik broj
		SessionSearch->MaxSearchResults = 100;
		// ako se ne spajamo preko LAN-a onda moramo ovo namjestiti da se namjesti Search Presence
		SessionSearch->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);
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

		TArray<FServerData> ServerNames;
		
		if (SearchResults.Num() > 0)
		{
			for (FOnlineSessionSearchResult& SearchResult : SearchResults)
			{
				UE_LOG(LogTemp, Warning, TEXT("Session ID: %s"), *SearchResult.GetSessionIdStr());
				FServerData Data;

				Data.HostUsername = SearchResult.Session.OwningUserName;
				Data.MaxPlayers = SearchResult.Session.SessionSettings.NumPublicConnections;
				Data.CurrentPlayers = Data.MaxPlayers - SearchResult.Session.NumOpenPublicConnections;			// moramo oduzeti broj slobodnih konekcija od maksimalnog broja konekcija
				FString ServerName;
				if (SearchResult.Session.SessionSettings.Get(SERVER_NAME_SETTINGS_KEY, ServerName))
				{
					Data.Name = ServerName;
				}
				else
				{
					Data.Name = "Could not find server name";
				}
				ServerNames.Add(Data);
			}

			Menu->SetServerList(ServerNames);
		}
		else
		{
			FString WarningMessage = "No active sessions found";
			Menu->SetWarningMessage(WarningMessage);
			UE_LOG(LogTemp, Warning, TEXT("No active sessions found"));
		}	
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

// pozivat ce se iz LobbyGameMode
// StartSession kreira sesiju i onemogucava naknadna spajanja na tu konkretnu sesiju
// Postavlja status sesije "In Progress" stoga se nije moguce spojiti na nju
void UPuzzlePlatformsGameInstance::StartSession()
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->StartSession(SESSION_NAME);
	}
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



