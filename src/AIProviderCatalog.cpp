#include "AIProviderCatalog.h"

QVector<AIProviderOption> AIProviderCatalog::all()
{
    QVector<AIProviderOption> list;

    {
        AIProviderOption o;
        o.id = QStringLiteral("nanobanana");
        o.displayName = QStringLiteral("Nano Banana (Google Gemini)");
        o.models = {
            QStringLiteral("gemini-3.1-flash-image"),
            QStringLiteral("gemini-3.1-flash-lite-image"),
            QStringLiteral("gemini-3-pro-image"),
            QStringLiteral("gemini-2.5-flash-image"),
            QStringLiteral("nano-banana-pro-preview"),
        };
        o.aspectRatios = {
            QStringLiteral("1:1"), QStringLiteral("16:9"), QStringLiteral("9:16"),
            QStringLiteral("4:3"), QStringLiteral("3:4"), QStringLiteral("3:2"),
            QStringLiteral("2:3"), QStringLiteral("21:9"),
        };
        o.imageSizes = {
            QStringLiteral("1K"), QStringLiteral("2K"), QStringLiteral("4K"),
            QStringLiteral("512"),
        };
        o.notes = QStringLiteral(
            "Google Gemini native image gen/edit. Use 1K/2K/4K (512 is Flash-only; "
            "Pro and nano-banana-pro reject it). If Google returns IMAGE_RECITATION, "
            "the app retries at a larger size — or try a more original prompt.");
        o.supportsEdit = true;
        list.push_back(o);
    }

    {
        AIProviderOption o;
        o.id = QStringLiteral("openai");
        o.displayName = QStringLiteral("OpenAI");
        o.models = {
            QStringLiteral("dall-e-3"),
            QStringLiteral("dall-e-2"),
            QStringLiteral("gpt-image-1"),
        };
        o.sizes = {
            QStringLiteral("1024x1024"),
            QStringLiteral("1792x1024"),
            QStringLiteral("1024x1792"),
            QStringLiteral("1536x1024"),
            QStringLiteral("1024x1536"),
            QStringLiteral("512x512"),
            QStringLiteral("256x256"),
        };
        o.qualities = {
            QStringLiteral("standard"), QStringLiteral("hd"),
            QStringLiteral("auto"), QStringLiteral("low"),
            QStringLiteral("medium"), QStringLiteral("high"),
        };
        o.notes = QStringLiteral(
            "DALL·E 3: 1024² / 1792×1024 / 1024×1792, quality standard|hd. "
            "DALL·E 2: 256 / 512 / 1024. "
            "gpt-image-1: 1024² / 1024×1536 / 1536×1024, quality low|medium|high|auto.");
        o.supportsEdit = true;
        list.push_back(o);
    }

    {
        AIProviderOption o;
        o.id = QStringLiteral("stability");
        o.displayName = QStringLiteral("Stability AI");
        o.models = {QStringLiteral("stable-diffusion-xl-1024-v1-0")};
        o.sizes = {
            QStringLiteral("1024x1024"),
            QStringLiteral("1152x896"),
            QStringLiteral("1216x832"),
            QStringLiteral("1344x768"),
            QStringLiteral("1536x640"),
            QStringLiteral("640x1536"),
            QStringLiteral("768x1344"),
            QStringLiteral("832x1216"),
            QStringLiteral("896x1152"),
        };
        o.notes = QStringLiteral(
            "SDXL text-to-image. Sizes must keep total pixels ≈ 1MP with valid SDXL dims.");
        o.supportsEdit = false;
        list.push_back(o);
    }

    {
        AIProviderOption o;
        o.id = QStringLiteral("higgsfield");
        o.displayName = QStringLiteral("Higgsfield");
        o.models = {
            QStringLiteral("gpt_image_2"),
            QStringLiteral("soul_2_0"),
            QStringLiteral("seedream_4_5"),
            QStringLiteral("z_image"),
            QStringLiteral("nano_banana_2"),
        };
        o.aspectRatios = {
            QStringLiteral("1:1"), QStringLiteral("16:9"), QStringLiteral("9:16"),
            QStringLiteral("4:3"), QStringLiteral("3:4"),
        };
        o.notes = QStringLiteral(
            "Runs via local higgsfield CLI. Resolution/aspect depend on the selected job model.");
        o.supportsEdit = true;
        list.push_back(o);
    }

    {
        AIProviderOption o;
        o.id = QStringLiteral("remotesd");
        o.displayName = QStringLiteral("Remote Stable Diffusion");
        o.models = {QStringLiteral("server-default")};
        o.sizes = {
            QStringLiteral("512x512"),
            QStringLiteral("768x768"),
            QStringLiteral("1024x1024"),
            QStringLiteral("768x512"),
            QStringLiteral("512x768"),
        };
        o.notes = QStringLiteral(
            "HTTP SD server (AI/remoteSDUrl). Exact options depend on your backend.");
        o.supportsEdit = false;
        list.push_back(o);
    }

    return list;
}

AIProviderOption AIProviderCatalog::byId(const QString &id)
{
    for (const auto &o : all()) {
        if (o.id == id)
            return o;
    }
    return all().isEmpty() ? AIProviderOption{} : all().first();
}

QStringList AIProviderCatalog::providerIds()
{
    QStringList ids;
    for (const auto &o : all())
        ids << o.id;
    return ids;
}
