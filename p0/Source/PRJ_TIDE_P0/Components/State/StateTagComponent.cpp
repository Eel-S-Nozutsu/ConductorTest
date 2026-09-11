// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#include "StateTagComponent.h"

void UStateTagComponent::AddStateTag( const FGameplayTag& Tag )
{
	if ( !Tag.IsValid() ) return;

	// マップにタグが存在しなければ 0 を基準に 1 追加、存在すればインクリメント
	int32& Count = TagCountMap.FindOrAdd( Tag, 0 );
	Count++;

	if ( Count == 1 )
	{
		ActiveStateTags.AddTag( Tag );
	}
}

void UStateTagComponent::RemoveStateTag( const FGameplayTag& Tag )
{
	if ( !Tag.IsValid() ) return;

	if ( int32* CountPtr = TagCountMap.Find( Tag ) )
	{
		// 0以下にならないよう保護する（二重削除防止）
		if ( *CountPtr > 0 )
		{
			( *CountPtr )--;
		}

		if ( *CountPtr <= 0 )
		{
			TagCountMap.Remove( Tag );
			ActiveStateTags.RemoveTag( Tag );
		}
	}
}

bool UStateTagComponent::HasStateTag( const FGameplayTag& Tag ) const
{
	return ActiveStateTags.HasTag( Tag );
}

void UStateTagComponent::ForceRemoveStateTagsByParent( const FGameplayTag& ParentTag )
{
	if ( !ParentTag.IsValid() ) return;

	// イテレート中のマップ変更（要素削除）によるクラッシュを防ぐため、削除対象のタグを一度配列に集める
	TArray<FGameplayTag> TagsToRemove;

	for ( const auto& Pair : TagCountMap )
	{
		// MatchesTag は、Pair.Key が ParentTag と完全に一致するか、その子タグである場合に true を返します
		if ( Pair.Key.MatchesTag( ParentTag ) )
		{
			TagsToRemove.Add( Pair.Key );
		}
	}

	// 集めたタグをコンテナとマップの両方から完全に消し去る（カウントもリセット）
	for ( const FGameplayTag& Tag : TagsToRemove )
	{
		TagCountMap.Remove( Tag );
		ActiveStateTags.RemoveTag( Tag );
	}
}
