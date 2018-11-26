// Fill out your copyright notice in the Description page of Project Settings.

#include "LobbyGameMode.h"

#include "Engine/Engine.h"

void ALobbyGameMode::PostLogin(APlayerController * NewPlayer)
{
	// osiguramo se da se PostLogin na baznoj klasi poziva
	Super::PostLogin(NewPlayer);
	++NumberOfPlayers;

	if (NumberOfPlayers >= 3)
	{
		UWorld* World = GetWorld();
		if (!ensure(World != nullptr)) return;

		// napravi seamless travel na zeljeni level
		bUseSeamlessTravel = true;
		World->ServerTravel("/Game/PuzzlePlatforms/Maps/Game?listen");
	}
}

void ALobbyGameMode::Logout(AController * Exiting)
{
	// osiguramo se da se Logout na baznoj klasi poziva
	Super::Logout(Exiting);
	--NumberOfPlayers;
}
