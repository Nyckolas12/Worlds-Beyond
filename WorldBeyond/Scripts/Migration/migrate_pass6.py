"""
Worlds Beyond - pass 6: the animated duo (Bond) meter.

Run with the editor closed, after building the C++:
    UnrealEditor-Cmd.exe <path>/WorldBeyond.uproject -run=pythonscript -script=<path>/Scripts/Migration/migrate_pass6.py -unattended -nosplash -NullRHI

Builds the three UI materials the Bond meter widget (UBeyondBondMeterWidget) draws with, in
/Game/WorldsBeyond/UI/DuoMeter/:
- M_UI_BondArc       the curved meter: steel frame, blue + purple energy on the left flowing into solid gold on the
                     right, crackling filaments, a bright front, a flash on every gain and a sweeping shine when full;
- M_UI_DuoFlames     blue / purple / gold flames circling the Heaven's Judgment medallion when the meter is full;
- M_UI_DuoMedallion  the round medallions (Angel's staff glyph over a turning rune ring, Ji-Woong's sun, the duo bolt
                     or the duo ability's own icon) in the style of the ability bar icons.
Everything is procedural (HLSL in Custom nodes) apart from two textures already in the project: the HUD's
crescent-staff glyph and the FX pack's magic circle. The materials are rebuilt on every run, so fixes land; the
widget's colours, sizes and glyphs are set from C++ (UBeyondBondMeterWidget) or a Blueprint child of it.
Every asset saved is copied to Saved/MigrationBackups/<timestamp>/ first. Running it twice is safe.
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.join(os.path.abspath(unreal.Paths.project_dir()), "Scripts", "Migration"))
from migration_common import ASSET_TOOLS, EAL, load, log, save, warn, write_report  # noqa: E402

MEL = unreal.MaterialEditingLibrary

FOLDER = "/Game/WorldsBeyond/UI/DuoMeter"
ANGEL_GLYPH = "/Game/WorldsBeyond/Blueprints/Widgets/Images/crescent-staff"
RUNE = "/Game/FXVarietyPack/Textures/T_ky_magicCircle020"

BLUE = (0.10, 0.45, 1.0)
PURPLE = (0.55, 0.15, 1.0)
GOLD = (1.0, 0.72, 0.12)


# ---------------------------------------------------------------- HLSL

ARC_HLSL = r"""
struct FBondArcFunctions
{
	float Hash(float2 P)
	{
		P = frac(P * float2(123.34, 456.21));
		P += dot(P, P + 45.32);
		return frac(P.x * P.y);
	}
	float Noise(float2 P)
	{
		float2 I = floor(P);
		float2 G = frac(P);
		G = G * G * (3.0 - 2.0 * G);
		float A = Hash(I);
		float B = Hash(I + float2(1.0, 0.0));
		float C = Hash(I + float2(0.0, 1.0));
		float D = Hash(I + float2(1.0, 1.0));
		return lerp(lerp(A, B, G.x), lerp(C, D, G.x), G.y);
	}
	float Fbm(float2 P)
	{
		float Sum = 0.0;
		float Amp = 0.5;
		for (int K = 0; K < 4; K++)
		{
			Sum += Amp * Noise(P);
			P = P * 2.03 + float2(17.1, 9.2);
			Amp *= 0.5;
		}
		return Sum;
	}
	// Arc of radius Ra around +Y with half aperture (Sc = sin, cos), rounded caps, half thickness Rb (iq)
	float SdArc(float2 P, float2 Sc, float Ra, float Rb)
	{
		P.x = abs(P.x);
		return ((Sc.y * P.x > Sc.x * P.y) ? length(P - Sc * Ra) : abs(length(P) - Ra)) - Rb;
	}
};
FBondArcFunctions Fn;

// Widget pixels; the arc centre lies below the image, Y up from it
float2 Px = UV * Size.xy;
float2 Q = float2(Px.x - Arc.x, Arc.y - Px.y);
float2 Sc = float2(sin(HalfAngle), cos(HalfAngle));
float HalfBand = Band.x * 0.5;
float InnerHalf = max(HalfBand - Band.y, 1.0);
float OuterD = Fn.SdArc(Q, Sc, Arc.z, HalfBand);
float InnerD = Fn.SdArc(Q, Sc, Arc.z, InnerHalf);
float OuterA = saturate(0.5 - OuterD);
float InnerA = saturate(0.5 - InnerD);

float Radius = length(Q);
float Angle = atan2(Q.x, Q.y);
float T = saturate(Angle / (2.0 * HalfAngle) + 0.5);
float Across = clamp((Radius - Arc.z) / InnerHalf, -1.0, 1.0);
float ArcLength = 2.0 * HalfAngle * Arc.z;
float Along = T * ArcLength;

// Frame: gunmetal with a warm inner lip and light bevels on both rims
float Bevel = saturate((Radius - Arc.z) / HalfBand * 0.5 + 0.5);
float3 FrameCol = float3(0.30, 0.31, 0.35) * (0.6 + 0.6 * Bevel) + float3(0.12, 0.08, 0.03) * (1.0 - Bevel);
FrameCol += 0.28 * saturate(1.0 - abs(OuterD + 1.5) / 1.5);
FrameCol *= 0.88 + 0.24 * Fn.Noise(Px * 0.35);

// Empty track
float3 TrackCol = float3(0.02, 0.025, 0.06) + 0.035 * Fn.Noise(float2(Along * 0.05 - Time * 0.2, Across * 2.0));

// Fill: blue and purple swirling together on the left, solid gold on the right
float Flow = Time * FlowSpeed;
float2 E = float2(Along / max(Band.x, 1.0), Across);
float N1 = Fn.Fbm(float2(E.x * 0.6 - Flow, E.y * 0.8 + Flow * 0.15));
float N2 = Fn.Fbm(float2(E.x * 1.3 - Flow * 1.7 + 5.2, E.y * 1.6 - Flow * 0.3));
float Mix = saturate(T / max(Stops.x, 0.01) * 0.7 + (N1 - 0.5) * 1.3);
float3 Col = lerp(ColorBlue.rgb, ColorPurple.rgb, Mix);
float GoldT = smoothstep(Stops.x, Stops.y, T + (N2 - 0.5) * 0.06);
Col = lerp(Col, ColorGold.rgb, GoldT);

float Core = 1.0 - 0.45 * Across * Across;
float Filament = 1.0 - saturate(abs(Fn.Noise(float2(E.x * 0.9 - Flow * 2.6, E.y * 1.4 + sin(Time * 6.0) * 0.15)) - 0.5) * 14.0);
float Flicker = 0.6 + 0.4 * Fn.Noise(float2(Time * 9.0, 3.7));
float Energy = (0.55 + 0.75 * N1 + 0.35 * N2) * Core;
float3 FillCol = Col * Energy + lerp(Col, float3(1.0, 1.0, 1.0), 0.6) * Filament * Flicker * 0.8;

// Bright front where the fill ends, flashing on every gain
float FrontDist = abs(T - Fill) * ArcLength;
float Front = exp(-FrontDist / 10.0) * step(T, Fill + 0.02) * (1.0 - step(0.999, Fill));
FillCol += lerp(Col, float3(1.0, 1.0, 1.0), 0.7) * Front * (0.8 + 1.2 * Surge);
FillCol *= 1.0 + 0.35 * Surge;

// Full: a shine sweeping left to right
float Sweep = frac(Time * 0.45) * 1.4 - 0.2;
float ShineX = (T - Sweep) * 9.0;
FillCol += float3(1.0, 0.95, 0.85) * exp(-ShineX * ShineX) * Ready * 0.6;

float FillMask = InnerA * saturate((Fill - T) * ArcLength * 0.5 + 0.5);

float3 Rgb = lerp(FrameCol, TrackCol, InnerA);
Rgb = lerp(Rgb, FillCol, FillMask);

// Soft halo in the fill colour, stronger when full
float Halo = exp(-max(OuterD, 0.0) / 7.0) * (1.0 - OuterA) * (0.18 + 0.55 * Ready) * step(T, Fill + 0.01) * step(0.001, Fill);
float OutA = saturate(OuterA + Halo * (1.0 - OuterA));
float3 OutRgb = (Rgb * OuterA + Col * Halo * (1.0 - OuterA)) / max(OutA, 0.0001);
return float4(OutRgb, OutA);
"""

FLAMES_HLSL = r"""
struct FDuoFlameFunctions
{
	float Hash(float3 P)
	{
		P = frac(P * 0.3183099 + 0.1);
		P *= 17.0;
		return frac(P.x * P.y * P.z * (P.x + P.y + P.z));
	}
	float Noise(float3 P)
	{
		float3 I = floor(P);
		float3 G = frac(P);
		G = G * G * (3.0 - 2.0 * G);
		float X00 = lerp(Hash(I), Hash(I + float3(1.0, 0.0, 0.0)), G.x);
		float X10 = lerp(Hash(I + float3(0.0, 1.0, 0.0)), Hash(I + float3(1.0, 1.0, 0.0)), G.x);
		float X01 = lerp(Hash(I + float3(0.0, 0.0, 1.0)), Hash(I + float3(1.0, 0.0, 1.0)), G.x);
		float X11 = lerp(Hash(I + float3(0.0, 1.0, 1.0)), Hash(I + float3(1.0, 1.0, 1.0)), G.x);
		return lerp(lerp(X00, X10, G.y), lerp(X01, X11, G.y), G.z);
	}
	float Fbm(float3 P)
	{
		float Sum = 0.0;
		float Amp = 0.55;
		for (int K = 0; K < 3; K++)
		{
			Sum += Amp * Noise(P);
			P = P * 2.1 + float3(3.1, 7.7, 1.3);
			Amp *= 0.5;
		}
		return Sum;
	}
	// Blue -> purple -> gold -> back to blue as X goes 0 -> 1
	float3 Palette(float X, float3 A, float3 B, float3 C)
	{
		X = frac(X) * 3.0;
		float3 Col = lerp(A, B, smoothstep(0.0, 1.0, X));
		Col = lerp(Col, C, smoothstep(1.0, 2.0, X));
		return lerp(Col, A, smoothstep(2.0, 3.0, X));
	}
};
FDuoFlameFunctions Fn;

float2 P = (UV - 0.5) * 2.0;
float R = length(P);
float Theta = atan2(P.y, P.x);
float Spin = Time * Speed;

// Flame tongues: seamless noise around the ring, scrolling outward, the whole ring turning
float2 Dir = float2(cos(Theta - Spin), sin(Theta - Spin));
float N = Fn.Fbm(float3(Dir * 3.0, R * 5.0 - Time * 2.6));
float Tongue = Fn.Fbm(float3(Dir * 6.5 + 11.0, R * 3.0 - Time * 3.4));
float Height = min((0.12 + 0.55 * N * N + 0.20 * Tongue) * (0.7 + 0.3 * Intensity), 0.58);
float Outward = R - RingRadius;
float Streak = Fn.Noise(float3(Dir * 9.0 + 5.0, R * 9.0 - Time * 4.5));
float Body = smoothstep(Height, Height * 0.15, Outward) * smoothstep(-0.06, 0.015, Outward) * (0.5 + 0.65 * Streak);
float Heat = saturate(1.0 - Outward / max(Height, 0.001));

float3 Col = Fn.Palette((Theta - Spin * 1.5) / 6.2831853 + 0.5, ColorBlue.rgb, ColorPurple.rgb, ColorGold.rgb);
Col = lerp(Col, float3(1.0, 1.0, 1.0), Heat * Heat * 0.35);

// A thin bright ring hugging the medallion and a soft glow around everything
float Rim = exp(-abs(Outward) * 60.0) * 0.6;
float Glow = exp(-max(Outward, 0.0) * 9.0) * step(0.0, Outward) * 0.15;

// Embers drifting outward, orbiting a little faster than the flames
float Cell = (Theta - Spin * 2.0) / 6.2831853 * 14.0;
float Sector = floor(Cell);
float Life = frac(Time * 0.7 + Fn.Hash(float3(Sector, 3.0, 1.0)));
float EmberR = RingRadius + 0.05 + Life * 0.38;
float2 EmberD = float2((frac(Cell) - 0.5) * 6.2831853 / 14.0 * R, R - EmberR);
float Ember = exp(-dot(EmberD, EmberD) * 2200.0) * (1.0 - Life) * step(0.35, Fn.Hash(float3(Sector, 7.0, 5.0)));

float Alpha = saturate((Body * (0.8 + 0.6 * Heat) + Rim + Glow + Ember) * Intensity);
float3 Rgb = Col * (0.9 + 0.7 * Heat) + float3(1.0, 0.9, 0.7) * Ember;
return float4(Rgb, Alpha);
"""

MEDALLION_HLSL = r"""
struct FMedallionFunctions
{
	// Signed distance to a lightning bolt (negative inside), iq's polygon SDF
	float Bolt(float2 P)
	{
		float2 V[7] = { float2(0.10, -0.60), float2(0.32, -0.60), float2(0.08, -0.08), float2(0.28, -0.08),
		                float2(-0.16, 0.62), float2(-0.02, 0.06), float2(-0.22, 0.06) };
		float D = dot(P - V[0], P - V[0]);
		float S = 1.0;
		int J = 6;
		for (int I = 0; I < 7; I++)
		{
			float2 Edge = V[J] - V[I];
			float2 W = P - V[I];
			float2 B = W - Edge * saturate(dot(W, Edge) / dot(Edge, Edge));
			D = min(D, dot(B, B));
			bool C1 = P.y >= V[I].y;
			bool C2 = P.y < V[J].y;
			bool C3 = Edge.x * W.y > Edge.y * W.x;
			if ((C1 && C2 && C3) || (!C1 && !C2 && !C3))
			{
				S = -S;
			}
			J = I;
		}
		return S * sqrt(D);
	}
	// Sun: a disc and twelve pointed rays around it (approximate distance, negative inside)
	float Sun(float2 P)
	{
		float R = length(P);
		float A = atan2(P.y, P.x);
		float Disc = R - 0.24;
		float RayEnd = 0.31 + 0.30 * pow(saturate(0.5 + 0.5 * cos(A * 12.0)), 3.0);
		float Rays = max(0.31 - R, R - RayEnd);
		return min(Disc, Rays);
	}
	float Fill(float D, float W)
	{
		return saturate(0.5 - D / W);
	}
};
FMedallionFunctions Fn;

float2 P = (UV - 0.5) * 2.0;
float R = length(P);
float Aa = max(fwidth(R), 0.002);

// Inner disc: radial gradient like the ability bar icons, a soft sheen at the top left
float3 Inner = Tint.rgb * (1.25 - 0.85 * saturate(R / 0.75));
Inner += Tint.rgb * 0.35 * saturate(1.0 - length(P - float2(-0.25, -0.30)) / 0.55);
Inner *= 0.8 + 0.4 * Glow;

// Rune ring turning slowly behind the glyph
float Turn = Time * 0.25;
float2 RP = float2(P.x * cos(Turn) - P.y * sin(Turn), P.x * sin(Turn) + P.y * cos(Turn));
float4 RuneS = Texture2DSample(Rune, RuneSampler, RP / 0.74 * 0.5 + 0.5);
float RuneMask = RuneS.a * max(RuneS.r, max(RuneS.g, RuneS.b)) * step(R, 0.74);
Inner = lerp(Inner, lerp(Tint.rgb, float3(1.0, 1.0, 1.0), 0.5), RuneMask * RuneStrength);

// Glyph: texture (0), sun (1) or lightning bolt (2)
float2 GUV = P / max(GlyphScale, 0.05) * 0.5 + 0.5;
float4 GS = Texture2DSample(Glyph, GlyphSampler, GUV);
float InBounds = step(0.0, GUV.x) * step(GUV.x, 1.0) * step(0.0, GUV.y) * step(GUV.y, 1.0);
float GlyphMask = GS.a * max(GS.r, max(GS.g, GS.b)) * InBounds;
float ShapeD = Mode < 1.5 ? Fn.Sun(P / max(GlyphScale, 0.05) * 0.62) : Fn.Bolt(P / max(GlyphScale, 0.05) * 0.62);
float ShapeMask = Fn.Fill(ShapeD, Aa);
GlyphMask = Mode < 0.5 ? GlyphMask : ShapeMask;
float GlyphGlow = Mode < 0.5 ? 0.0 : exp(-max(ShapeD, 0.0) * 18.0) * (1.0 - ShapeMask) * 0.6 * Glow;
Inner = lerp(Inner, GlyphColor.rgb, GlyphMask);
Inner += lerp(Tint.rgb, float3(1.0, 1.0, 1.0), 0.4) * GlyphGlow;

// Steel rim with a dark inner line, and four diamond studs (the concept's frame)
float RimD = abs(R - 0.81) - 0.07;
float RimBevel = saturate((R - 0.74) / 0.14);
float3 RimCol = float3(0.33, 0.34, 0.38) * (0.65 + 0.6 * RimBevel) + float3(0.10, 0.07, 0.03) * (1.0 - RimBevel);
RimCol *= 1.0 - 0.6 * exp(-abs(R - 0.745) * 90.0);
float2 Fold = abs(P);
float2 Stud = float2(min(Fold.x, Fold.y), max(Fold.x, Fold.y));
float StudD = (Stud.x / 0.07 + abs(Stud.y - 0.86) / 0.12 - 1.0) * 0.07;
float3 StudCol = float3(0.55, 0.56, 0.60) * (0.75 + 0.5 * saturate(0.5 - P.y));

float InnerA = Fn.Fill(R - 0.75, Aa);
float RimA = Fn.Fill(RimD, Aa);
float StudA = Fn.Fill(StudD, Aa);
float3 Rgb = lerp(Inner, RimCol, RimA);
Rgb = lerp(Rgb, StudCol, StudA);
float Alpha = max(max(InnerA, RimA), StudA);

// Dim (greyed) and the outer glow in the medallion's colour
float Grey = dot(Rgb, float3(0.3, 0.59, 0.11));
Rgb = lerp(Rgb, float3(Grey, Grey, Grey) * 0.55, Dim);
float Halo = exp(-max(R - 0.88, 0.0) * 25.0) * (1.0 - Alpha) * Glow * 0.7 * (1.0 - Dim);
float OutA = saturate(Alpha + Halo);
float3 OutRgb = (Rgb * Alpha + Tint.rgb * Halo) / max(OutA, 0.0001);
return float4(OutRgb, OutA);
"""


# ---------------------------------------------------------------- material building

def _node(material, cls, x, y, **props):
    expression = MEL.create_material_expression(material, cls, x, y)
    for key, value in props.items():
        expression.set_editor_property(key, value)
    return expression


def _link(source, target, target_input):
    if not MEL.connect_material_expressions(source, "", target, target_input):
        warn("could not connect %s -> %s.%s" % (source.get_name(), target.get_name(), target_input or "Input"))
        return False
    return True


def _sampler_type(texture):
    """The sampler type a texture parameter needs for this texture (a mismatch is a material compile error)."""
    types = unreal.MaterialSamplerType
    try:
        compression = texture.get_editor_property("compression_settings").name
        srgb = texture.get_editor_property("srgb")
    except Exception:
        return types.SAMPLERTYPE_COLOR
    if compression in ("TC_GRAYSCALE", "TC_DISPLACEMENTMAP"):
        return types.SAMPLERTYPE_GRAYSCALE if srgb else types.SAMPLERTYPE_LINEAR_GRAYSCALE
    if compression == "TC_ALPHA":
        return types.SAMPLERTYPE_ALPHA
    if compression == "TC_MASKS":
        return types.SAMPLERTYPE_MASKS
    if compression == "TC_NORMALMAP":
        return types.SAMPLERTYPE_NORMAL
    if compression in ("TC_HDR", "TC_HDR_COMPRESSED", "TC_HALF_FLOAT", "TC_SINGLE_FLOAT", "TC_VECTOR_DISPLACEMENTMAP"):
        return types.SAMPLERTYPE_LINEAR_COLOR
    return types.SAMPLERTYPE_COLOR if srgb else types.SAMPLERTYPE_LINEAR_COLOR


def build_ui_material(name, code, inputs, description):
    """
    A User Interface material whose colour and opacity come from one Custom node.
    inputs: (input name, kind, default) with kind uv / time / scalar / vector / texture; parameters take the input name.
    """
    path = "%s/%s" % (FOLDER, name)
    if EAL.does_asset_exist(path):
        material = load(path)
        if material is None:
            return None
        MEL.delete_all_material_expressions(material)
        action = "rebuilt"
    else:
        material = ASSET_TOOLS.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
        if material is None:
            warn("could not create %s" % path)
            return None
        action = "created"

    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

    custom = _node(material, unreal.MaterialExpressionCustom, -350, 0)
    custom.set_editor_property("code", code)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    custom.set_editor_property("description", description)
    custom_inputs = []
    for input_name, _kind, _default in inputs:
        custom_input = unreal.CustomInput()
        custom_input.set_editor_property("input_name", input_name)
        custom_inputs.append(custom_input)
    custom.set_editor_property("inputs", custom_inputs)

    y = -400
    for input_name, kind, default in inputs:
        if kind == "uv":
            source = _node(material, unreal.MaterialExpressionTextureCoordinate, -800, y)
        elif kind == "time":
            source = _node(material, unreal.MaterialExpressionTime, -800, y)
        elif kind == "scalar":
            source = _node(material, unreal.MaterialExpressionScalarParameter, -800, y,
                           parameter_name=input_name, default_value=float(default))
        elif kind == "vector":
            value = tuple(default) + (1.0,) * (4 - len(default))
            source = _node(material, unreal.MaterialExpressionVectorParameter, -800, y,
                           parameter_name=input_name, default_value=unreal.LinearColor(*value))
        elif kind == "texture":
            texture = load(default)
            source = _node(material, unreal.MaterialExpressionTextureObjectParameter, -800, y, parameter_name=input_name)
            if texture:
                source.set_editor_property("texture", texture)
                source.set_editor_property("sampler_type", _sampler_type(texture))
            else:
                warn("%s: default texture %s not found for %s" % (name, default, input_name))
        else:
            raise ValueError(kind)
        _link(source, custom, input_name)
        y += 110

    color = _node(material, unreal.MaterialExpressionComponentMask, -120, -60, r=True, g=True, b=True, a=False)
    alpha = _node(material, unreal.MaterialExpressionComponentMask, -120, 60, r=False, g=False, b=False, a=True)
    for mask in (color, alpha):
        if not MEL.connect_material_expressions(custom, "", mask, "") and not _link(custom, mask, "Input"):
            warn("%s: the Custom node isn't connected to its masks" % name)
    if not MEL.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        warn("%s: could not connect Final Color" % name)
    if not MEL.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY):
        warn("%s: could not connect Opacity" % name)

    MEL.recompile_material(material)
    save(material, path)
    log("%s %s (%d parameters)" % (action, path, len([i for i in inputs if i[1] not in ("uv", "time")])))
    return material


def step_duo_meter_materials():
    build_ui_material("M_UI_BondArc", ARC_HLSL, [
        ("UV", "uv", None),
        ("Time", "time", None),
        ("Size", "vector", (640.0, 150.0, 0.0)),
        ("Arc", "vector", (320.0, 640.0, 600.0)),
        ("HalfAngle", "scalar", 0.497),
        ("Band", "vector", (34.0, 7.0, 0.0)),
        ("Fill", "scalar", 0.65),
        ("Ready", "scalar", 0.0),
        ("Surge", "scalar", 0.0),
        ("FlowSpeed", "scalar", 0.6),
        ("Stops", "vector", (0.4, 0.62, 0.0)),
        ("ColorBlue", "vector", BLUE),
        ("ColorPurple", "vector", PURPLE),
        ("ColorGold", "vector", GOLD),
    ], "Bond arc")

    build_ui_material("M_UI_DuoFlames", FLAMES_HLSL, [
        ("UV", "uv", None),
        ("Time", "time", None),
        ("RingRadius", "scalar", 0.38),
        ("Intensity", "scalar", 1.0),
        ("Speed", "scalar", 0.9),
        ("ColorBlue", "vector", BLUE),
        ("ColorPurple", "vector", PURPLE),
        ("ColorGold", "vector", GOLD),
    ], "Duo flames")

    build_ui_material("M_UI_DuoMedallion", MEDALLION_HLSL, [
        ("UV", "uv", None),
        ("Time", "time", None),
        ("Glyph", "texture", ANGEL_GLYPH),
        ("Rune", "texture", RUNE),
        ("Mode", "scalar", 0.0),
        ("GlyphScale", "scalar", 0.62),
        ("RuneStrength", "scalar", 0.0),
        ("Glow", "scalar", 0.5),
        ("Dim", "scalar", 0.0),
        ("Tint", "vector", BLUE),
        ("GlyphColor", "vector", (1.0, 1.0, 1.0)),
    ], "Duo medallion")


def main():
    log("pass 6")
    if not hasattr(unreal, "MaterialExpressionCustom"):
        warn("this editor build has no MaterialExpressionCustom in Python; nothing was changed")
        write_report("last_run_pass6.txt")
        return
    step_duo_meter_materials()
    write_report("last_run_pass6.txt")


main()
