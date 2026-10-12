// Fill out your copyright notice in the Description page of Project Settings.

#include "World/BeyondGroundLibrary.h"
#include "WorldBeyondEditor.h"
#include "AssetCompilingManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Async/ParallelFor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "LandscapeEditTypes.h"
#include "LandscapeGrassType.h"
#include "LandscapeProxy.h"
#include "LandscapeStreamingProxy.h"
#include "LandscapeSubsystem.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAbs.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionDesaturation.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionLandscapeGrassOutput.h"
#include "Materials/MaterialExpressionLandscapeLayerBlend.h"
#include "Materials/MaterialExpressionLandscapeLayerCoords.h"
#include "Materials/MaterialExpressionLandscapeLayerSample.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionPixelDepth.h"
#include "Materials/MaterialExpressionPower.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTransform.h"
#include "Materials/MaterialExpressionUtils.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialInstanceConstant.h"
#include "ShaderCompiler.h"
#include "World/BeyondHeightfieldGenerator.h"
#include "World/BeyondWorldBuilderInternal.h"
#include "World/BeyondWorldBuilderLibrary.h"
#include "World/BeyondWorldLayout.h"

namespace BeyondGroundLocal
{
	// One output of a material node
	struct FPin
	{
		UMaterialExpression* Expression = nullptr;
		int32 Output = 0;

		FPin() = default;
		FPin(UMaterialExpression* InExpression, int32 InOutput = 0) : Expression(InExpression), Output(InOutput) {}
		bool IsSet() const { return Expression != nullptr; }
	};

	// Texture sample outputs: RGB, R, G, B, A
	int32 ChannelOutput(EBeyondGroundChannel Channel)
	{
		switch (Channel)
		{
		case EBeyondGroundChannel::G: return 2;
		case EBeyondGroundChannel::B: return 3;
		case EBeyondGroundChannel::A: return 4;
		default: return 1;
		}
	}

	void Wire(FExpressionInput& Input, const FPin& Pin)
	{
		if (Pin.IsSet())
		{
			Input.Connect(Pin.Output, Pin.Expression);
		}
	}

	// A named input of a Custom node
	struct FNamedPin
	{
		const TCHAR* Name;
		FPin Pin;
	};

	/** Builds a material graph; nodes go in rows (one per ground layer) so the graph reads left to right in the editor */
	class FGraph
	{
	public:
		explicit FGraph(UMaterial* InMaterial) : Material(InMaterial) {}

		void Row(int32 InRow)
		{
			CurrentRow = InRow;
			Placed = 0;
		}

		template <typename T>
		T* Make()
		{
			const int32 X = -9000 + (Placed % 30) * 280;
			const int32 Y = CurrentRow * 1400 + (Placed / 30) * 200;
			++Placed;
			return Cast<T>(UMaterialEditingLibrary::CreateMaterialExpression(Material, T::StaticClass(), X, Y));
		}

		FPin Const(float Value)
		{
			UMaterialExpressionConstant* E = Make<UMaterialExpressionConstant>();
			E->R = Value;
			return E;
		}

		FPin Const3(const FLinearColor& Value)
		{
			UMaterialExpressionConstant3Vector* E = Make<UMaterialExpressionConstant3Vector>();
			E->Constant = Value;
			return E;
		}

		FPin Scalar(const FString& Name, const FString& Group, float Value, int32 Priority)
		{
			UMaterialExpressionScalarParameter* E = Make<UMaterialExpressionScalarParameter>();
			E->ParameterName = FName(*Name);
			E->Group = FName(*Group);
			E->SortPriority = Priority;
			E->DefaultValue = Value;
			return E;
		}

		// RGB of a colour parameter
		FPin Colour(const FString& Name, const FString& Group, const FLinearColor& Value, int32 Priority)
		{
			UMaterialExpressionVectorParameter* E = Make<UMaterialExpressionVectorParameter>();
			E->ParameterName = FName(*Name);
			E->Group = FName(*Group);
			E->SortPriority = Priority;
			E->DefaultValue = Value;
			return FPin(E, 0);
		}

		template <typename T>
		FPin Binary(const FPin& A, const FPin& B)
		{
			T* E = Make<T>();
			Wire(E->A, A);
			Wire(E->B, B);
			return E;
		}

		FPin Mul(const FPin& A, const FPin& B) { return Binary<UMaterialExpressionMultiply>(A, B); }
		FPin Add(const FPin& A, const FPin& B) { return Binary<UMaterialExpressionAdd>(A, B); }
		FPin Sub(const FPin& A, const FPin& B) { return Binary<UMaterialExpressionSubtract>(A, B); }
		FPin Div(const FPin& A, const FPin& B) { return Binary<UMaterialExpressionDivide>(A, B); }
		FPin Append(const FPin& A, const FPin& B) { return Binary<UMaterialExpressionAppendVector>(A, B); }
		FPin Dot(const FPin& A, const FPin& B) { return Binary<UMaterialExpressionDotProduct>(A, B); }

		FPin Lerp(const FPin& A, const FPin& B, const FPin& Alpha)
		{
			UMaterialExpressionLinearInterpolate* E = Make<UMaterialExpressionLinearInterpolate>();
			Wire(E->A, A);
			Wire(E->B, B);
			Wire(E->Alpha, Alpha);
			return E;
		}

		template <typename T>
		FPin Unary(const FPin& X)
		{
			T* E = Make<T>();
			Wire(E->Input, X);
			return E;
		}

		FPin Saturate(const FPin& X) { return Unary<UMaterialExpressionSaturate>(X); }
		FPin Abs(const FPin& X) { return Unary<UMaterialExpressionAbs>(X); }
		FPin OneMinus(const FPin& X) { return Unary<UMaterialExpressionOneMinus>(X); }

		FPin Mask(const FPin& X, bool R, bool G, bool B, bool A = false)
		{
			UMaterialExpressionComponentMask* E = Make<UMaterialExpressionComponentMask>();
			Wire(E->Input, X);
			E->R = R;
			E->G = G;
			E->B = B;
			E->A = A;
			return E;
		}

		FPin Power(const FPin& Base, const FPin& Exponent)
		{
			UMaterialExpressionPower* E = Make<UMaterialExpressionPower>();
			Wire(E->Base, Base);
			Wire(E->Exponent, Exponent);
			return E;
		}

		FPin Desaturate(const FPin& X, const FPin& Fraction)
		{
			UMaterialExpressionDesaturation* E = Make<UMaterialExpressionDesaturation>();
			Wire(E->Input, X);
			Wire(E->Fraction, Fraction);
			return E;
		}

		FPin Transform(const FPin& X, EMaterialVectorCoordTransformSource From, EMaterialVectorCoordTransform To)
		{
			UMaterialExpressionTransform* E = Make<UMaterialExpressionTransform>();
			Wire(E->Input, X);
			E->TransformSourceType = From;
			E->TransformType = To;
			return E;
		}

		FPin Custom(const FString& Description, const FString& Code, ECustomMaterialOutputType Output, const TArray<FNamedPin>& Inputs)
		{
			UMaterialExpressionCustom* E = Make<UMaterialExpressionCustom>();
			E->Description = Description;
			E->Code = Code;
			E->OutputType = Output;
			E->Inputs.Reset();
			for (const FNamedPin& Input : Inputs)
			{
				FCustomInput& Slot = E->Inputs.AddDefaulted_GetRef();
				Slot.InputName = Input.Name;
				Wire(Slot.Input, Input.Pin);
			}
			return E;
		}

		// A texture parameter sampled at UV (RGB output); the same name may be sampled at several UVs
		FPin Texture(const FString& Name, const FString& Group, UTexture* Texture, const FPin& UV, int32 Priority,
			ESamplerSourceMode Source = SSM_Wrap_WorldGroupSettings)
		{
			UMaterialExpressionTextureSampleParameter2D* E = Make<UMaterialExpressionTextureSampleParameter2D>();
			E->ParameterName = FName(*Name);
			E->Group = FName(*Group);
			E->SortPriority = Priority;
			E->Texture = Texture;
			E->SamplerType = MaterialExpressionUtils::GetSamplerTypeForTexture(Texture);
			E->SamplerSource = Source;
			Wire(E->Coordinates, UV);
			return FPin(E, 0);
		}

		// The landscape's own coordinates, in metres (one quad is a metre)
		FPin Coords()
		{
			if (!CoordsPin.IsSet())
			{
				UMaterialExpressionLandscapeLayerCoords* E = Make<UMaterialExpressionLandscapeLayerCoords>();
				E->MappingType = TCMT_XY;
				E->MappingScale = 1.0f;
				CoordsPin = E;
			}
			return CoordsPin;
		}

		FPin WorldPosition()
		{
			if (!WorldPositionPin.IsSet())
			{
				WorldPositionPin = Make<UMaterialExpressionWorldPosition>();
			}
			return WorldPositionPin;
		}

		FPin VertexNormal()
		{
			if (!VertexNormalPin.IsSet())
			{
				VertexNormalPin = Make<UMaterialExpressionVertexNormalWS>();
			}
			return VertexNormalPin;
		}

		FPin Depth()
		{
			if (!DepthPin.IsSet())
			{
				DepthPin = Make<UMaterialExpressionPixelDepth>();
			}
			return DepthPin;
		}

		// A layer's painted weight (0..1)
		FPin LayerWeight(FName Layer)
		{
			if (const FPin* Found = Weights.Find(Layer))
			{
				return *Found;
			}
			UMaterialExpressionLandscapeLayerSample* E = Make<UMaterialExpressionLandscapeLayerSample>();
			E->ParameterName = Layer;
			return Weights.Add(Layer, FPin(E));
		}

		UMaterial* Material;

	private:
		int32 CurrentRow = 0;
		int32 Placed = 0;
		FPin CoordsPin;
		FPin WorldPositionPin;
		FPin VertexNormalPin;
		FPin DepthPin;
		TMap<FName, FPin> Weights;
	};

	// A layer's surface, ready for the layer blends
	struct FSurface
	{
		FPin BaseColour;
		FPin Normal;
		FPin Roughness;
		// After the look's contrast / offset (the height blend's input)
		FPin Height;
		// Before them (where snow settles)
		FPin RawHeight;
	};

	// What every layer can use
	struct FShared
	{
		FPin FarAlpha;
		FPin FarScale;
		FPin FarMix;
		FPin FarNormalFade;
		FPin GlowFarFade;
		FPin Frost;
		FPin Molten;
		FPin Blight;
		// Where none of the other three is: the Elderwood
		FPin Forest;
		UTexture* MacroNoise = nullptr;
		// Surfaces built so far, for the layers that turn into another (snow, ash, corruption, grass patches)
		TMap<FName, FSurface> Built;
		FPin RockSnowStart;
		FPin RockSnowEnd;
		FPin GapSnowLevel;
		bool bHasSnow = false;
		FSurface Snow;
		bool bHasTrampled = false;
		FSurface Trampled;
	};

	const TCHAR* TriplanarWeightsCode = TEXT("float3 W = pow(abs(N), 4.0);\nreturn W / max(dot(W, float3(1.0, 1.0, 1.0)), 1e-4);");

	// Whiteout blend of three tangent-space normals projected along X (uv = zy), Y (xz) and Z (xy); world space out
	const TCHAR* TriplanarNormalCode = TEXT(
		"float3 W = pow(abs(N), 4.0);\n"
		"W /= max(dot(W, float3(1.0, 1.0, 1.0)), 1e-4);\n"
		"float3 TX = float3(NX.xy + N.zy, abs(NX.z) * N.x);\n"
		"float3 TY = float3(NY.xy + N.xz, abs(NY.z) * N.y);\n"
		"float3 TZ = float3(NZ.xy + N.xy, abs(NZ.z) * N.z);\n"
		"return normalize(TX.zyx * W.x + TY.xzy * W.y + TZ.xyz * W.z);");

	FString ParamName(const FString& Prefix, const TCHAR* What)
	{
		return FString::Printf(TEXT("%s %s"), *Prefix, What);
	}

	FPin Luminance(FGraph& G, const FPin& Colour)
	{
		return G.Dot(Colour, G.Const3(FLinearColor(0.3f, 0.59f, 0.11f)));
	}

	FSurface BuildFlat(FGraph& G, const FBeyondGroundLayerLook& Look, const FString& Prefix, const FString& Group, const FShared& Shared, const FPin& TileMetres,
		const FLinearColor& Fallback)
	{
		FSurface Out;
		const FPin UV = G.Div(G.Coords(), TileMetres);
		FPin Near;
		if (Look.BaseColor)
		{
			Near = G.Texture(ParamName(Prefix, TEXT("Base Colour")), Group, Look.BaseColor, UV, 1);
			Out.BaseColour = Near;
			if (Shared.FarScale.IsSet())
			{
				// Far away the colour is also read from a much larger repeat, turned, so the tiles don't line up into a grid
				const FPin FarUV = G.Custom(TEXT("Far UV"), TEXT("return float2(UV.x * 0.8 - UV.y * 0.6, UV.x * 0.6 + UV.y * 0.8) * Scale;"), CMOT_Float2,
					{ { TEXT("UV"), UV }, { TEXT("Scale"), Shared.FarScale } });
				const FPin Far = G.Texture(ParamName(Prefix, TEXT("Base Colour")), Group, Look.BaseColor, FarUV, 1);
				Out.BaseColour = G.Lerp(Near, Far, G.Mul(Shared.FarAlpha, Shared.FarMix));
			}
		}
		else
		{
			Near = G.Const3(Fallback);
			Out.BaseColour = Near;
		}
		Out.Normal = Look.Normal ? G.Texture(ParamName(Prefix, TEXT("Normal")), Group, Look.Normal, UV, 2) : G.Const3(FLinearColor(0.0f, 0.0f, 1.0f));
		Out.Roughness = Look.RoughnessMap && Look.RoughnessChannel != EBeyondGroundChannel::None
			? FPin(G.Texture(ParamName(Prefix, TEXT("Roughness Map")), Group, Look.RoughnessMap, UV, 3).Expression, ChannelOutput(Look.RoughnessChannel))
			: FPin();
		Out.RawHeight = Look.HeightMap && Look.HeightChannel != EBeyondGroundChannel::None
			? FPin(G.Texture(ParamName(Prefix, TEXT("Height Map")), Group, Look.HeightMap, UV, 4).Expression, ChannelOutput(Look.HeightChannel))
			: Luminance(G, Near);
		return Out;
	}

	FSurface BuildTriplanar(FGraph& G, const FBeyondGroundLayerLook& Look, const FString& Prefix, const FString& Group, const FShared& Shared,
		const FPin& TileMetres, const FLinearColor& Fallback)
	{
		FSurface Out;
		const FPin P = G.Div(G.WorldPosition(), G.Mul(TileMetres, G.Const(100.0f)));
		const FPin X = G.Mask(P, true, false, false);
		const FPin Y = G.Mask(P, false, true, false);
		const FPin Z = G.Mask(P, false, false, true);
		const FPin UVs[3] = { G.Append(Z, Y), G.Append(X, Z), G.Mask(P, true, true, false) };
		const FPin N = G.VertexNormal();
		const FPin W = G.Custom(TEXT("Triplanar weights"), TriplanarWeightsCode, CMOT_Float3, { { TEXT("N"), N } });
		const FPin Weights[3] = { G.Mask(W, true, false, false), G.Mask(W, false, true, false), G.Mask(W, false, false, true) };

		auto Blend = [&G, &Weights](const FPin Samples[3])
		{
			return G.Add(G.Add(G.Mul(Samples[0], Weights[0]), G.Mul(Samples[1], Weights[1])), G.Mul(Samples[2], Weights[2]));
		};

		if (Look.BaseColor)
		{
			const FPin Colours[3] = {
				G.Texture(ParamName(Prefix, TEXT("Base Colour")), Group, Look.BaseColor, UVs[0], 1),
				G.Texture(ParamName(Prefix, TEXT("Base Colour")), Group, Look.BaseColor, UVs[1], 1),
				G.Texture(ParamName(Prefix, TEXT("Base Colour")), Group, Look.BaseColor, UVs[2], 1) };
			Out.BaseColour = Blend(Colours);
			if (Shared.FarScale.IsSet())
			{
				// Cliffs are seen from far away: a larger repeat mixed in with distance
				const FPin Far[3] = {
					G.Texture(ParamName(Prefix, TEXT("Base Colour")), Group, Look.BaseColor, G.Mul(UVs[0], Shared.FarScale), 1),
					G.Texture(ParamName(Prefix, TEXT("Base Colour")), Group, Look.BaseColor, G.Mul(UVs[1], Shared.FarScale), 1),
					G.Texture(ParamName(Prefix, TEXT("Base Colour")), Group, Look.BaseColor, G.Mul(UVs[2], Shared.FarScale), 1) };
				Out.BaseColour = G.Lerp(Out.BaseColour, Blend(Far), G.Mul(Shared.FarAlpha, Shared.FarMix));
			}
		}
		else
		{
			Out.BaseColour = G.Const3(Fallback);
		}
		if (Look.Normal)
		{
			const FPin NX = G.Texture(ParamName(Prefix, TEXT("Normal")), Group, Look.Normal, UVs[0], 2);
			const FPin NY = G.Texture(ParamName(Prefix, TEXT("Normal")), Group, Look.Normal, UVs[1], 2);
			const FPin NZ = G.Texture(ParamName(Prefix, TEXT("Normal")), Group, Look.Normal, UVs[2], 2);
			const FPin World = G.Custom(TEXT("Triplanar normal"), TriplanarNormalCode, CMOT_Float3,
				{ { TEXT("NX"), NX }, { TEXT("NY"), NY }, { TEXT("NZ"), NZ }, { TEXT("N"), N } });
			Out.Normal = G.Transform(World, TRANSFORMSOURCE_World, TRANSFORM_Tangent);
		}
		else
		{
			Out.Normal = G.Const3(FLinearColor(0.0f, 0.0f, 1.0f));
		}
		if (Look.RoughnessMap && Look.RoughnessChannel != EBeyondGroundChannel::None)
		{
			const int32 Channel = ChannelOutput(Look.RoughnessChannel);
			const FPin Rough[3] = {
				FPin(G.Texture(ParamName(Prefix, TEXT("Roughness Map")), Group, Look.RoughnessMap, UVs[0], 3).Expression, Channel),
				FPin(G.Texture(ParamName(Prefix, TEXT("Roughness Map")), Group, Look.RoughnessMap, UVs[1], 3).Expression, Channel),
				FPin(G.Texture(ParamName(Prefix, TEXT("Roughness Map")), Group, Look.RoughnessMap, UVs[2], 3).Expression, Channel) };
			Out.Roughness = Blend(Rough);
		}
		if (Look.HeightMap && Look.HeightChannel != EBeyondGroundChannel::None)
		{
			const int32 Channel = ChannelOutput(Look.HeightChannel);
			const FPin Heights[3] = {
				FPin(G.Texture(ParamName(Prefix, TEXT("Height Map")), Group, Look.HeightMap, UVs[0], 4).Expression, Channel),
				FPin(G.Texture(ParamName(Prefix, TEXT("Height Map")), Group, Look.HeightMap, UVs[1], 4).Expression, Channel),
				FPin(G.Texture(ParamName(Prefix, TEXT("Height Map")), Group, Look.HeightMap, UVs[2], 4).Expression, Channel) };
			Out.RawHeight = Blend(Heights);
		}
		else
		{
			Out.RawHeight = Luminance(G, Out.BaseColour);
		}
		return Out;
	}

	/** A layer's surface from its look: textures, colour adjustment, roughness, normal strength, the height blend's input */
	FSurface BuildSurface(FGraph& G, const FBeyondGroundLayerLook& Look, const FString& Prefix, const FString& Group, const FShared& Shared,
		const FLinearColor& Fallback)
	{
		const FPin Tile = G.Scalar(ParamName(Prefix, TEXT("Tile Size")), Group, Look.TileSize, 0);
		FSurface Out = Look.bTriplanar ? BuildTriplanar(G, Look, Prefix, Group, Shared, Tile, Fallback) : BuildFlat(G, Look, Prefix, Group, Shared, Tile, Fallback);

		// Colour: desaturate, tint, brighten
		const FPin Saturation = G.Scalar(ParamName(Prefix, TEXT("Saturation")), Group, Look.Saturation, 10);
		const FPin Tint = G.Colour(ParamName(Prefix, TEXT("Tint")), Group, Look.Tint, 11);
		const FPin Brightness = G.Scalar(ParamName(Prefix, TEXT("Brightness")), Group, Look.Brightness, 12);
		Out.BaseColour = G.Mul(G.Mul(G.Desaturate(Out.BaseColour, G.OneMinus(Saturation)), Tint), Brightness);

		// Roughness: the map times a scale, or a constant
		if (Out.Roughness.IsSet())
		{
			Out.Roughness = G.Saturate(G.Mul(Out.Roughness, G.Scalar(ParamName(Prefix, TEXT("Roughness Scale")), Group, Look.RoughnessScale, 13)));
		}
		else
		{
			Out.Roughness = G.Scalar(ParamName(Prefix, TEXT("Roughness")), Group, Look.Roughness, 13);
		}

		// Normal strength, flattening with distance
		const FPin Flat = G.Const3(FLinearColor(0.0f, 0.0f, 1.0f));
		Out.Normal = G.Lerp(Flat, Out.Normal, G.Scalar(ParamName(Prefix, TEXT("Normal Strength")), Group, Look.NormalStrength, 14));
		if (Shared.FarAlpha.IsSet() && !Look.bTriplanar)
		{
			Out.Normal = G.Lerp(Out.Normal, Flat, G.Mul(Shared.FarAlpha, Shared.FarNormalFade));
		}

		// The height blend's input: (h - 0.5) * contrast + 0.5 + offset
		const FPin Contrast = G.Scalar(ParamName(Prefix, TEXT("Height Contrast")), Group, Look.HeightContrast, 15);
		const FPin Offset = G.Scalar(ParamName(Prefix, TEXT("Height Offset")), Group, Look.HeightOffset, 16);
		Out.Height = G.Saturate(G.Add(G.Mul(G.Sub(Out.RawHeight, G.Const(0.5f)), Contrast), G.Add(G.Const(0.5f), Offset)));
		return Out;
	}

	// The frost region's change to a layer (snow on paths, between cobbles, on rock tops)
	void ApplyFrost(FGraph& G, const FBeyondGroundLayerLook& Look, const FString& Prefix, const FString& Group, const FShared& Shared, FSurface& Out)
	{
		if (Look.FrostVariant == EBeyondGroundFrostVariant::None || !Shared.Frost.IsSet())
		{
			return;
		}
		const FPin Amount = G.Mul(Shared.Frost, G.Scalar(ParamName(Prefix, TEXT("Frost Amount")), Group, Look.FrostAmount, 20));
		const FSurface* Snow = nullptr;
		FPin Alpha;
		switch (Look.FrostVariant)
		{
		case EBeyondGroundFrostVariant::TrampledSnow:
			if (Shared.bHasTrampled)
			{
				Snow = &Shared.Trampled;
				Alpha = Amount;
			}
			break;
		case EBeyondGroundFrostVariant::SnowInGaps:
			if (Shared.bHasSnow)
			{
				Snow = &Shared.Snow;
				Alpha = G.Mul(Amount, G.Saturate(G.Mul(G.Sub(Shared.GapSnowLevel, Out.RawHeight), G.Const(8.0f))));
			}
			break;
		case EBeyondGroundFrostVariant::SnowOnTop:
			if (Shared.bHasSnow)
			{
				Snow = &Shared.Snow;
				const FPin Up = G.Mask(G.VertexNormal(), false, false, true);
				Alpha = G.Mul(Amount, G.Saturate(G.Div(G.Sub(Up, Shared.RockSnowStart), G.Sub(Shared.RockSnowEnd, Shared.RockSnowStart))));
			}
			break;
		default:
			break;
		}
		if (!Snow)
		{
			return;
		}
		Out.BaseColour = G.Lerp(Out.BaseColour, Snow->BaseColour, Alpha);
		Out.Normal = G.Lerp(Out.Normal, Snow->Normal, Alpha);
		Out.Roughness = G.Lerp(Out.Roughness, Snow->Roughness, Alpha);
		Out.Height = G.Lerp(Out.Height, Snow->Height, Alpha);
	}

	void BlendTo(FGraph& G, FSurface& Out, const FSurface& Other, const FPin& Alpha)
	{
		Out.BaseColour = G.Lerp(Out.BaseColour, Other.BaseColour, Alpha);
		Out.Normal = G.Lerp(Out.Normal, Other.Normal, Alpha);
		Out.Roughness = G.Lerp(Out.Roughness, Other.Roughness, Alpha);
		Out.Height = G.Lerp(Out.Height, Other.Height, Alpha);
	}

	// Patches of another layer through this one (Elderwood only), then each region's turn into another layer
	void ApplyBlends(FGraph& G, const FBeyondGroundLayerLook& Look, const FString& Prefix, const FString& Group, const FShared& Shared, FSurface& Out)
	{
		const FSurface* Patch = Look.PatchLayer.IsNone() ? nullptr : Shared.Built.Find(Look.PatchLayer);
		if (Patch && Look.PatchAmount > 0.0f && Shared.MacroNoise)
		{
			const FPin UV = G.Div(G.Coords(), G.Scalar(ParamName(Prefix, TEXT("Patch Tile Size")), Group, Look.PatchTileSize, 51));
			const FPin Noise(G.Texture(TEXT("Macro Noise"), TEXT("Variation"), Shared.MacroNoise, UV, 0).Expression, 1);
			const FPin Amount = G.Scalar(ParamName(Prefix, TEXT("Patch Amount")), Group, Look.PatchAmount, 50);
			const FPin Sharpness = G.Scalar(ParamName(Prefix, TEXT("Patch Sharpness")), Group, Look.PatchSharpness, 52);
			FPin Alpha = G.Saturate(G.Mul(G.Sub(Noise, G.OneMinus(Amount)), Sharpness));
			if (Shared.Forest.IsSet())
			{
				Alpha = G.Mul(Alpha, Shared.Forest);
			}
			BlendTo(G, Out, *Patch, Alpha);
		}
		struct FRegionBlend
		{
			const TCHAR* Name;
			FName Layer;
			float Amount;
			FPin Weight;
		};
		const FRegionBlend Blends[] = {
			{ TEXT("Frost Layer Amount"), Look.FrostLayer, Look.FrostLayerAmount, Shared.Frost },
			{ TEXT("Molten Layer Amount"), Look.MoltenLayer, Look.MoltenLayerAmount, Shared.Molten },
			{ TEXT("Blight Layer Amount"), Look.BlightLayer, Look.BlightLayerAmount, Shared.Blight } };
		int32 Priority = 60;
		for (const FRegionBlend& Blend : Blends)
		{
			const FSurface* Other = Blend.Layer.IsNone() ? nullptr : Shared.Built.Find(Blend.Layer);
			if (Other && Blend.Amount > 0.0f && Blend.Weight.IsSet())
			{
				BlendTo(G, Out, *Other, G.Mul(Blend.Weight, G.Scalar(ParamName(Prefix, Blend.Name), Group, Blend.Amount, Priority)));
			}
			++Priority;
		}
	}

	// A layer's glow (embers, veins), weighted by where the layer is painted and fading with distance
	FPin BuildGlow(FGraph& G, const FBeyondGroundLayerLook& Look, const FString& Prefix, const FString& Group, const FShared& Shared)
	{
		if (Look.Glow == EBeyondGroundGlow::None || !Look.GlowMask)
		{
			return FPin();
		}
		const FPin UV = G.Div(G.Coords(), G.Scalar(ParamName(Prefix, TEXT("Glow Tile Size")), Group, Look.GlowTileSize, 30));
		const FPin Mask(G.Texture(ParamName(Prefix, TEXT("Glow Mask")), Group, Look.GlowMask, UV, 31).Expression, 1);
		const FPin Sharpness = G.Scalar(ParamName(Prefix, TEXT("Glow Sharpness")), Group, Look.GlowSharpness, 32);
		FPin Amount;
		if (Look.Glow == EBeyondGroundGlow::Embers)
		{
			Amount = G.Power(G.Saturate(Mask), Sharpness);
		}
		else
		{
			// Lines where the mask crosses the middle: 1 - |2m - 1| near 1, kept within the width
			const FPin Ridge = G.OneMinus(G.Abs(G.Sub(G.Mul(Mask, G.Const(2.0f)), G.Const(1.0f))));
			const FPin Line = G.Saturate(G.Div(G.Sub(Ridge, G.OneMinus(Sharpness)), Sharpness));
			Amount = G.Mul(Line, Line);
		}
		const FPin Colour = G.Colour(ParamName(Prefix, TEXT("Glow Colour")), Group, Look.GlowColour, 33);
		const FPin Strength = G.Scalar(ParamName(Prefix, TEXT("Glow Strength")), Group, Look.GlowStrength, 34);
		FPin Glow = G.Mul(G.Mul(G.Mul(Amount, Colour), Strength), G.LayerWeight(Look.Layer));
		if (Shared.FarAlpha.IsSet() && Shared.GlowFarFade.IsSet())
		{
			Glow = G.Mul(Glow, G.OneMinus(G.Mul(Shared.FarAlpha, Shared.GlowFarFade)));
		}
		return Glow;
	}

	void AddBlendInput(UMaterialExpressionLandscapeLayerBlend* Blend, FName Layer, const FPin& Value, const FPin& Height, bool bPreview)
	{
		FLayerBlendInput& Input = Blend->Layers.AddDefaulted_GetRef();
		Input.LayerName = Layer;
		// The blend type first: a layer that isn't height blended drops its height input
		Input.BlendType = LB_HeightBlend;
		Input.PreviewWeight = bPreview ? 1.0f : 0.0f;
		Input.ConstHeightInput = 0.5f;
		Wire(Input.LayerInput, Value);
		Wire(Input.HeightInput, Height);
	}

	void ConnectProperty(const FPin& Pin, EMaterialProperty Property)
	{
		if (!Pin.IsSet())
		{
			return;
		}
		const TArray<FExpressionOutput>& Outputs = Pin.Expression->GetOutputs();
		const FString OutputName = Outputs.IsValidIndex(Pin.Output) ? Outputs[Pin.Output].OutputName.ToString() : FString();
		UMaterialEditingLibrary::ConnectMaterialProperty(Pin.Expression, OutputName, Property);
	}
}

void UBeyondGroundLibrary::ClearMaterialGraph(UMaterial* Material)
{
	BeyondWorldBuilderLocal::ClearMaterialExpressions(Material);
}

TArray<FName> UBeyondGroundLibrary::GetLayerNames()
{
	TArray<FName> Names;
	for (int32 Layer = 0; Layer < BeyondTerrainLayers::Count; ++Layer)
	{
		Names.Add(BeyondTerrainLayers::GetName(Layer));
	}
	return Names;
}

UMaterial* UBeyondGroundLibrary::CreateGroundMaterial(const FString& AssetPath, const TArray<FBeyondGroundLayerLook>& Looks, const FBeyondGroundSettings& Settings,
	const TArray<FBeyondGroundGrassInput>& Grass, bool bRebuild)
{
	using namespace BeyondWorldBuilderLocal;
	using namespace BeyondGroundLocal;
	bool bNew = false;
	UMaterial* Material = MakeAsset<UMaterial>(AssetPath, bNew);
	if (!bNew && !bRebuild)
	{
		return Material;
	}
	ClearMaterialExpressions(Material);
	FGraph G(Material);

	auto FindLook = [&Looks](FName Layer) -> const FBeyondGroundLayerLook*
	{
		return Looks.FindByPredicate([Layer](const FBeyondGroundLayerLook& Look) { return Look.Layer == Layer; });
	};

	// Shared: distance fade, the region mask, the snow the frost uses
	FShared Shared;
	G.Row(-2);
	const FString Ground = TEXT("Ground");
	const FPin FarStart = G.Scalar(TEXT("Far Blend Start"), Ground, Settings.FarBlendStart, 1);
	const FPin FarEnd = G.Scalar(TEXT("Far Blend End"), Ground, Settings.FarBlendEnd, 2);
	Shared.FarAlpha = G.Saturate(G.Div(G.Sub(G.Depth(), FarStart), G.Sub(FarEnd, FarStart)));
	Shared.FarScale = G.Scalar(TEXT("Far Tile Scale"), Ground, Settings.FarTileScale, 3);
	Shared.FarMix = G.Scalar(TEXT("Far Mix"), Ground, Settings.FarMix, 4);
	Shared.FarNormalFade = G.Scalar(TEXT("Far Normal Fade"), Ground, Settings.FarNormalFade, 5);
	Shared.GlowFarFade = G.Scalar(TEXT("Glow Far Fade"), Ground, Settings.GlowFarFade, 6);
	Shared.MacroNoise = Settings.MacroNoise;

	const FString Regions = TEXT("Regions");
	if (Settings.RegionMask)
	{
		const FPin MaskUV = G.Div(G.Coords(), G.Scalar(TEXT("Region Mask Size"), Regions, Settings.RegionMaskSize, 1));
		const FPin Mask = G.Texture(TEXT("Region Mask"), Regions, Settings.RegionMask, MaskUV, 0, SSM_Clamp_WorldGroupSettings);
		Shared.Frost = FPin(Mask.Expression, 1);
		Shared.Molten = FPin(Mask.Expression, 2);
		Shared.Blight = FPin(Mask.Expression, 3);
		Shared.Forest = G.Saturate(G.Sub(G.Const(1.0f), G.Add(G.Add(Shared.Frost, Shared.Molten), Shared.Blight)));
	}
	const FString Frost = TEXT("Frost");
	Shared.RockSnowStart = G.Scalar(TEXT("Rock Snow Start"), Frost, Settings.RockSnowStart, 40);
	Shared.RockSnowEnd = G.Scalar(TEXT("Rock Snow End"), Frost, Settings.RockSnowEnd, 41);
	Shared.GapSnowLevel = G.Scalar(TEXT("Gap Snow Level"), Frost, Settings.GapSnowLevel, 42);

	if (Settings.TrampledSnowBaseColor)
	{
		G.Row(-1);
		FBeyondGroundLayerLook Trampled;
		Trampled.Layer = TEXT("Trampled Snow");
		Trampled.BaseColor = Settings.TrampledSnowBaseColor;
		Trampled.Normal = Settings.TrampledSnowNormal;
		Trampled.RoughnessMap = Settings.TrampledSnowRoughness;
		Trampled.RoughnessChannel = Settings.TrampledSnowRoughnessChannel;
		Trampled.HeightChannel = EBeyondGroundChannel::None;
		Trampled.TileSize = Settings.TrampledSnowTileSize;
		Trampled.Roughness = 0.6f;
		Shared.Trampled = BuildSurface(G, Trampled, TEXT("Trampled Snow"), Frost, Shared, FLinearColor(0.8f, 0.82f, 0.86f));
		Shared.bHasTrampled = true;
	}

	// The layers others turn into (snow, ash, corruption, grass patches) first, so they can be borrowed
	TArray<FSurface> Surfaces;
	Surfaces.SetNum(BeyondTerrainLayers::Count);
	const int32 SnowIndex = GetLayerNames().IndexOfByKey(Settings.SnowLayer);
	TSet<FName> Borrowed = { Settings.SnowLayer };
	for (const FBeyondGroundLayerLook& Look : Looks)
	{
		Borrowed.Append({ Look.FrostLayer, Look.MoltenLayer, Look.BlightLayer, Look.PatchLayer });
	}
	TArray<int32> Order;
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		for (int32 Layer = 0; Layer < BeyondTerrainLayers::Count; ++Layer)
		{
			if (Borrowed.Contains(BeyondTerrainLayers::GetName(Layer)) == (Pass == 0))
			{
				Order.Add(Layer);
			}
		}
	}
	FPin Glow;
	int32 Textured = 0;
	for (const int32 Layer : Order)
	{
		const FName Name = BeyondTerrainLayers::GetName(Layer);
		const FString Prefix = Name.ToString();
		G.Row(Layer);
		FBeyondGroundLayerLook Look;
		if (const FBeyondGroundLayerLook* Found = FindLook(Name))
		{
			Look = *Found;
			Textured += Look.BaseColor ? 1 : 0;
		}
		Look.Layer = Name;
		FSurface Surface = BuildSurface(G, Look, Prefix, Prefix, Shared, BeyondTerrainLayers::GetGreyboxColour(Layer));
		if (Layer == SnowIndex)
		{
			Shared.Snow = Surface;
			Shared.bHasSnow = true;
		}
		else
		{
			ApplyFrost(G, Look, Prefix, Prefix, Shared, Surface);
		}
		ApplyBlends(G, Look, Prefix, Prefix, Shared, Surface);
		Shared.Built.Add(Name, Surface);
		const FPin LayerGlow = BuildGlow(G, Look, Prefix, Prefix, Shared);
		if (LayerGlow.IsSet())
		{
			Glow = Glow.IsSet() ? G.Add(Glow, LayerGlow) : LayerGlow;
		}
		Surfaces[Layer] = Surface;
	}

	// The three height blends, in the layers' order
	G.Row(BeyondTerrainLayers::Count + 1);
	UMaterialExpressionLandscapeLayerBlend* ColourBlend = G.Make<UMaterialExpressionLandscapeLayerBlend>();
	UMaterialExpressionLandscapeLayerBlend* NormalBlend = G.Make<UMaterialExpressionLandscapeLayerBlend>();
	UMaterialExpressionLandscapeLayerBlend* RoughnessBlend = G.Make<UMaterialExpressionLandscapeLayerBlend>();
	for (int32 Layer = 0; Layer < BeyondTerrainLayers::Count; ++Layer)
	{
		const FName Name = BeyondTerrainLayers::GetName(Layer);
		const FSurface& Surface = Surfaces[Layer];
		AddBlendInput(ColourBlend, Name, Surface.BaseColour, Surface.Height, Layer == 0);
		AddBlendInput(NormalBlend, Name, Surface.Normal, Surface.Height, Layer == 0);
		AddBlendInput(RoughnessBlend, Name, Surface.Roughness, Surface.Height, Layer == 0);
	}

	// Large-scale variation, then the regions' grades
	FPin Colour(ColourBlend);
	const FString Variation = TEXT("Variation");
	if (Settings.MacroNoise)
	{
		const FPin A(G.Texture(TEXT("Macro Noise"), Variation, Settings.MacroNoise,
			G.Div(G.Coords(), G.Scalar(TEXT("Macro Tile Size"), Variation, Settings.MacroTileSize, 1)), 0).Expression, 1);
		const FPin B(G.Texture(TEXT("Macro Noise"), Variation, Settings.MacroNoise,
			G.Div(G.Coords(), G.Scalar(TEXT("Macro Tile Size 2"), Variation, Settings.MacroTileSize2, 2)), 0).Expression, 1);
		const FPin Strength = G.Scalar(TEXT("Macro Strength"), Variation, Settings.MacroStrength, 3);
		// 1 + (a + b - 1) * strength
		Colour = G.Mul(Colour, G.Add(G.Const(1.0f), G.Mul(G.Sub(G.Add(A, B), G.Const(1.0f)), Strength)));
		// Drier and lusher stretches over the larger scale
		const FPin TintA = G.Colour(TEXT("Variation Tint A"), Variation, Settings.VariationTintA, 4);
		const FPin TintB = G.Colour(TEXT("Variation Tint B"), Variation, Settings.VariationTintB, 5);
		Colour = G.Mul(Colour, G.Lerp(TintA, TintB, G.Saturate(G.Sub(G.Mul(B, G.Const(1.6f)), G.Const(0.3f)))));
	}
	struct FGradeEntry
	{
		const TCHAR* Name;
		FPin Weight;
		const FBeyondGroundRegionGrade* Values;
	};
	const FGradeEntry Grades[] = {
		{ TEXT("Forest"), Shared.Forest, &Settings.Forest },
		{ TEXT("Frost"), Shared.Frost, &Settings.Frost },
		{ TEXT("Molten"), Shared.Molten, &Settings.Molten },
		{ TEXT("Blight"), Shared.Blight, &Settings.Blight } };
	for (const FGradeEntry& Grade : Grades)
	{
		const FPin Weight = Grade.Weight;
		const FBeyondGroundRegionGrade& Values = *Grade.Values;
		if (!Weight.IsSet())
		{
			continue;
		}
		const FString Prefix = Grade.Name;
		const FPin Saturation = G.Scalar(ParamName(Prefix, TEXT("Saturation")), Regions, Values.Saturation, 10);
		const FPin Tint = G.Colour(ParamName(Prefix, TEXT("Tint")), Regions, Values.Tint, 11);
		const FPin Brightness = G.Scalar(ParamName(Prefix, TEXT("Brightness")), Regions, Values.Brightness, 12);
		const FPin Graded = G.Mul(G.Mul(G.Desaturate(Colour, G.OneMinus(Saturation)), Tint), Brightness);
		Colour = G.Lerp(Colour, Graded, Weight);
	}

	ConnectProperty(Colour, MP_BaseColor);
	ConnectProperty(FPin(NormalBlend), MP_Normal);
	ConnectProperty(FPin(RoughnessBlend), MP_Roughness);
	ConnectProperty(Glow, MP_EmissiveColor);

	// Grass grown where its layer is painted
	int32 GrassTypes = 0;
	if (Grass.Num() > 0)
	{
		UMaterialExpressionLandscapeGrassOutput* GrassOutput = G.Make<UMaterialExpressionLandscapeGrassOutput>();
		GrassOutput->GrassTypes.Reset();
		for (const FBeyondGroundGrassInput& Entry : Grass)
		{
			if (!Entry.GrassType || Entry.Layer.IsNone() || Entry.Name.IsNone() || GrassTypes >= UMaterialExpressionLandscapeGrassOutput::MaxGrassTypes)
			{
				continue;
			}
			FGrassInput& Input = GrassOutput->GrassTypes.Emplace_GetRef(Entry.Name);
			Input.GrassType = Entry.GrassType;
			FPin Weight = G.LayerWeight(Entry.Layer);
			const FLinearColor& Share = Entry.Regions;
			if (Shared.Forest.IsSet() && !(Share.R == 1.0f && Share.G == 1.0f && Share.B == 1.0f && Share.A == 1.0f))
			{
				// How much grows in each region: the region weights mixed by the entry's shares
				struct FTerm
				{
					float Amount;
					FPin Region;
				};
				const FTerm Terms[] = { { Share.R, Shared.Forest }, { Share.G, Shared.Frost }, { Share.B, Shared.Molten }, { Share.A, Shared.Blight } };
				FPin Sum;
				for (const FTerm& Term : Terms)
				{
					if (Term.Amount > 0.0f)
					{
						const FPin Part = Term.Amount == 1.0f ? Term.Region : G.Mul(Term.Region, G.Const(Term.Amount));
						Sum = Sum.IsSet() ? G.Add(Sum, Part) : Part;
					}
				}
				Weight = Sum.IsSet() ? G.Mul(Weight, Sum) : G.Const(0.0f);
			}
			Wire(Input.Input, Weight);
			++GrassTypes;
		}
	}

	UMaterialEditingLibrary::RecompileMaterial(Material);
	if (bNew)
	{
		FAssetRegistryModule::AssetCreated(Material);
	}
	Material->MarkPackageDirty();
	UE_LOG(LogBeyondEditor, Display, TEXT("Ground: built %s (%d layers, %d textured, %d grass types)"), *AssetPath,
		static_cast<int32>(BeyondTerrainLayers::Count), Textured, GrassTypes);
	return Material;
}

UMaterialInstanceConstant* UBeyondGroundLibrary::CreateGroundMaterialInstance(const FString& AssetPath, UMaterialInterface* Parent, bool bReset)
{
	using namespace BeyondWorldBuilderLocal;
	if (!Parent)
	{
		return nullptr;
	}
	bool bNew = false;
	UMaterialInstanceConstant* Instance = MakeAsset<UMaterialInstanceConstant>(AssetPath, bNew);
	if (!bNew && !bReset && Instance->Parent == Parent)
	{
		return Instance;
	}
	Instance->PreEditChange(nullptr);
	Instance->SetParentEditorOnly(Parent);
	if (bReset)
	{
		Instance->ClearParameterValuesEditorOnly();
	}
	Instance->PostEditChange();
	if (bNew)
	{
		FAssetRegistryModule::AssetCreated(Instance);
	}
	Instance->MarkPackageDirty();
	return Instance;
}

UTexture2D* UBeyondGroundLibrary::BakeRegionMask(UBeyondWorldLayout* Layout, const FString& AssetPath, int32 Resolution)
{
	using namespace BeyondWorldBuilderLocal;
	const FBeyondHeightfield* Field = GetField(Layout);
	if (!Field)
	{
		return nullptr;
	}
	Resolution = FMath::Clamp(Resolution, 64, 2048);
	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(Resolution * Resolution);
	const float MetresPerPixel = (Field->Size - 1) / static_cast<float>(Resolution);
	ParallelFor(Resolution, [&](int32 PY)
	{
		for (int32 PX = 0; PX < Resolution; ++PX)
		{
			// Row 0 is the north edge, like the landscape's +Y running south
			const FVector2D P(-Field->HalfExtent + (PX + 0.5f) * MetresPerPixel, Field->HalfExtent - (PY + 0.5f) * MetresPerPixel);
			float Frost = 0.0f;
			float Molten = 0.0f;
			float Blight = 0.0f;
			for (int32 Region = 0; Region < Layout->Regions.Num(); ++Region)
			{
				const float W = Field->SampleRegionWeight(Region, P);
				switch (Layout->Regions[Region].Style)
				{
				case EBeyondTerrainStyle::Frost: Frost += W; break;
				case EBeyondTerrainStyle::Molten: Molten += W; break;
				case EBeyondTerrainStyle::Blight: Blight += W; break;
				default: break;
				}
			}
			auto Byte = [](float W) { return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(W * 255.0f), 0, 255)); };
			Pixels[PY * Resolution + PX] = FColor(Byte(Frost), Byte(Molten), Byte(Blight), 255);
		}
	});

	bool bNew = false;
	UTexture2D* Texture = MakeAsset<UTexture2D>(AssetPath, bNew);
	Texture->PreEditChange(nullptr);
	Texture->Source.Init(Resolution, Resolution, 1, 1, TSF_BGRA8, reinterpret_cast<const uint8*>(Pixels.GetData()));
	Texture->SRGB = false;
	Texture->CompressionSettings = TC_Masks;
	Texture->AddressX = TA_Clamp;
	Texture->AddressY = TA_Clamp;
	Texture->LODGroup = TEXTUREGROUP_World;
	Texture->MipGenSettings = TMGS_FromTextureGroup;
	Texture->NeverStream = true;
	Texture->VirtualTextureStreaming = false;
	Texture->PostEditChange();
	if (bNew)
	{
		FAssetRegistryModule::AssetCreated(Texture);
	}
	Texture->MarkPackageDirty();
	UE_LOG(LogBeyondEditor, Display, TEXT("Ground: baked the region mask %s (%d px)"), *AssetPath, Resolution);
	return Texture;
}

ULandscapeGrassType* UBeyondGroundLibrary::CreateGrassType(const FString& AssetPath, const TArray<FBeyondGrassVarietySpec>& Varieties)
{
	using namespace BeyondWorldBuilderLocal;
	bool bNew = false;
	ULandscapeGrassType* Grass = MakeAsset<ULandscapeGrassType>(AssetPath, bNew);
	Grass->PreEditChange(nullptr);
	Grass->GrassVarieties.Reset();
	for (const FBeyondGrassVarietySpec& Spec : Varieties)
	{
		if (!Spec.Mesh)
		{
			continue;
		}
		FGrassVariety& Variety = Grass->GrassVarieties.AddDefaulted_GetRef();
		Variety.GrassMesh = Spec.Mesh;
		Variety.OverrideMaterials = Spec.OverrideMaterials;
		// Both the per-platform and the per-quality values: which one is read depends on the project's grass settings
		Variety.GrassDensity = FPerPlatformFloat(Spec.Density);
		Variety.GrassDensityQuality = FPerQualityLevelFloat(Spec.Density);
		Variety.StartCullDistance = FPerPlatformInt(Spec.StartCullDistance);
		Variety.StartCullDistanceQuality = FPerQualityLevelInt(Spec.StartCullDistance);
		Variety.EndCullDistance = FPerPlatformInt(Spec.EndCullDistance);
		Variety.EndCullDistanceQuality = FPerQualityLevelInt(Spec.EndCullDistance);
		Variety.bUseGrid = false;
		Variety.Scaling = EGrassScaling::Uniform;
		Variety.ScaleX = FFloatInterval(Spec.ScaleMin, Spec.ScaleMax);
		Variety.ScaleY = Variety.ScaleX;
		Variety.ScaleZ = Variety.ScaleX;
		Variety.RandomRotation = true;
		Variety.AlignToSurface = Spec.bAlignToSurface;
		Variety.bCastDynamicShadow = Spec.bCastShadow;
		Variety.bAffectDistanceFieldLighting = false;
		Variety.bReceivesDecals = true;
	}
	Grass->PostEditChange();
	if (bNew)
	{
		FAssetRegistryModule::AssetCreated(Grass);
	}
	Grass->MarkPackageDirty();
	return Grass;
}

UMaterialInterface* UBeyondGroundLibrary::GetLandscapeMaterial(UObject* WorldContext)
{
	UWorld* World = BeyondWorldBuilderLocal::ResolveWorld(WorldContext);
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ALandscape> It(World); It; ++It)
	{
		return It->GetLandscapeMaterial();
	}
	return nullptr;
}

int32 UBeyondGroundLibrary::SetLandscapeMaterial(UObject* WorldContext, UMaterialInterface* Material)
{
	UWorld* World = BeyondWorldBuilderLocal::ResolveWorld(WorldContext);
	if (!World || !Material)
	{
		return 0;
	}
	// Every proxy holds its own copy and only syncs while loaded
	UBeyondWorldBuilderLibrary::LoadLandscapeProxies(World);
	FProperty* Property = FindFProperty<FProperty>(ALandscapeProxy::StaticClass(), GET_MEMBER_NAME_CHECKED(ALandscapeProxy, LandscapeMaterial));
	for (TActorIterator<ALandscape> It(World); It; ++It)
	{
		ALandscape* Landscape = *It;
		Landscape->PreEditChange(Property);
		Landscape->LandscapeMaterial = Material;
		FPropertyChangedEvent Event(Property, EPropertyChangeType::ValueSet);
		Landscape->PostEditChangeProperty(Event);
	}
	int32 Carrying = 0;
	for (TActorIterator<ALandscapeStreamingProxy> It(World); It; ++It)
	{
		ALandscapeStreamingProxy* Proxy = *It;
		if (Proxy->LandscapeMaterial != Material)
		{
			Proxy->Modify();
			Proxy->LandscapeMaterial = Material;
			Proxy->UpdateAllComponentMaterialInstances(true);
		}
		Proxy->MarkPackageDirty();
		++Carrying;
	}
	if (ULandscapeSubsystem* Landscapes = World->GetSubsystem<ULandscapeSubsystem>())
	{
		Landscapes->MarkModifiedLandscapesAsDirty(UE::Landscape::EBuildFlags::None);
	}
	UE_LOG(LogBeyondEditor, Display, TEXT("Ground: %s on the landscape and %d streaming proxies"), *Material->GetPathName(), Carrying);
	return Carrying;
}

int32 UBeyondGroundLibrary::GrowGrassAt(UObject* WorldContext, const TArray<FVector>& Cameras)
{
	UWorld* World = BeyondWorldBuilderLocal::ResolveWorld(WorldContext);
	ULandscapeSubsystem* Landscapes = World ? World->GetSubsystem<ULandscapeSubsystem>() : nullptr;
	if (!Landscapes)
	{
		return 0;
	}
	// The grass maps are rendered from the landscape material: its shaders must be there
	FAssetCompilingManager::Get().FinishAllCompilation();
	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}
	Landscapes->BuildGrassMaps(UE::Landscape::EBuildFlags::None);
	TArray<FVector> Views = Cameras;
	Landscapes->RegenerateGrass(true, true, TOptional<TArrayView<FVector>>(MakeArrayView(Views)));

	int32 Instances = 0;
	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		TInlineComponentArray<UInstancedStaticMeshComponent*> Components(*It);
		for (const UInstancedStaticMeshComponent* Component : Components)
		{
			Instances += Component->GetInstanceCount();
		}
	}
	UE_LOG(LogBeyondEditor, Display, TEXT("Ground: %d grass instances around %d views"), Instances, Cameras.Num());
	return Instances;
}
