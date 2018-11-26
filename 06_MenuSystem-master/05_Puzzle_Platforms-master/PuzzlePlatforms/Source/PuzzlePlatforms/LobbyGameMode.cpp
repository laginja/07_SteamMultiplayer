// Fill out your copyright notice in the Description page of Project Settings.

#include "LobbyGameMode.h"

#include "Engine/Engine.h"
#include "TimerManager.h"

#include "PuzzlePlatformsGameInstance.h"

void ALobbyGameMode::PostLogin(APlayerController * NewPlayer)
{
	// osiguramo se da se PostLogin na baznoj klasi poziva
	Super::PostLogin(NewPlayer);
	++NumberOfPlayers;

	// ako ima vise od 3 igraca, zapocni timer nakon kojeg ce se svi prebaciti na zeljeni level
	if (NumberOfPlayers >= 3)
	{
		GetWorldTimerManager().SetTimer(GameStartTimer, this, &ALobbyGameMode::StartGame, 10);
	}
}

void ALobbyGameMode::Logout(AController * Exiting)
{
	// osiguramo se da se Logout na baznoj klasi poziva
	Super::Logout(Exiting);
	--NumberOfPlayers;
}

void ALobbyGameMode::StartGame()
{
	auto GameInstance = Cast<UPuzzlePlatformsGameInstance>(GetGameInstance());

	if (GameInstance == nullptr) return;

	GameInstance->StartSession();
	UWorld* World = GetWorld();
	if (!ensure(World != nullptr)) return;

	// napravi seamless travel na zeljeni level
	bUseSeamlessTravel = true;
	World->ServerTravel("/Game/PuzzlePlatforms/Maps/Game?listen");
}
