#pragma once

#include "pch.h"
#include "json/json.h"
#include "ParseContext.h"
#include "ParseUtil.h"
#include "SemanticVersion.h"
#include "RemoteResourceInformation.h"

namespace AdaptiveSharedNamespace
{
    class ParseContext;

    class BaseElement
    {
    public:
        BaseElement() :
            m_additionalProperties{}, m_id{}, m_typeString{}, m_internalId{GetNextInternalId()}, m_requires(0),
            m_fallbackContent(nullptr), m_fallbackType(FallbackType::None)
        {
        }

        // Element type and identity
        std::string GetElementTypeString() const;
        void SetElementTypeString(const std::string& value);
        virtual std::string GetId() const;
        virtual void SetId(const std::string& value);
        unsigned int GetInternalId() const { return m_internalId; }
        // TODO: GetElementType?

        // Element parsing/serialization
        template<typename T>
        static void ParseJsonObject(ParseContext& context, const Json::Value& json, std::shared_ptr<BaseElement>& baseElement)
        {
            T::ParseJsonObject(context, json, baseElement);
        }

        template<typename T> void DeserializeBase(ParseContext& context, const Json::Value& json);

        template<typename T> static std::shared_ptr<T> Deserialize(ParseContext& context, const Json::Value& json) = 0;
        virtual std::string Serialize() const;
        virtual Json::Value SerializeToJsonValue() const;
        Json::Value GetAdditionalProperties() const;
        void SetAdditionalProperties(const Json::Value& additionalProperties);
        FallbackType GetFallbackType() const { return m_fallbackType; }

        bool MeetsRequirements(const std::unordered_map<std::string, std::string>& /*hostProvides*/) const
        {
            return true;
        }
        std::shared_ptr<BaseElement> GetFallbackContent() const { return m_fallbackContent; }

        // misc
        virtual void GetResourceInformation(std::vector<RemoteResourceInformation>& resourceUris);

    protected:
        // Element parsing/serialization
        void SetTypeString(const std::string& type) { m_typeString = type; }
        std::string m_typeString;
        Json::Value m_additionalProperties;

    private:
        static unsigned int GetNextInternalId()
        {
            static unsigned int nextId = 1;
            return nextId++;
        }

        template<typename T> void ParseFallback(ParseContext& context, const Json::Value& json);
        void ParseRequires(ParseContext& /*context*/, const Json::Value& json);

        std::unordered_map<std::string, SemanticVersion> m_requires;
        std::shared_ptr<BaseElement> m_fallbackContent;
        FallbackType m_fallbackType;
        std::string m_id;
        const unsigned int m_internalId;
    };

    template<typename T> void BaseElement::DeserializeBase(ParseContext& context, const Json::Value& json)
    {
        ParseUtil::ThrowIfNotJsonObject(json);
        SetId(ParseUtil::GetString(json, AdaptiveCardSchemaKey::Id));
        ParseFallback<T>(context, json);
        ParseRequires(context, json);
    }

    template<typename T> void BaseElement::ParseFallback(ParseContext& context, const Json::Value& json)
    {
        const auto fallbackValue = ParseUtil::ExtractJsonValue(json, AdaptiveCardSchemaKey::Fallback, false);
        if (!fallbackValue.empty())
        {
            if (fallbackValue.isString())
            {
                auto fallbackStringValue = ParseUtil::ToLowercase(fallbackValue.asString());
                if (fallbackStringValue == "drop")
                {
                    m_fallbackType = FallbackType::Drop;
                    return;
                }
                throw AdaptiveCardParseException(ErrorStatusCode::InvalidPropertyValue,
                                                 "The only valid string value for the fallback property is 'drop'.");
            }
            else if (fallbackValue.isObject())
            {
                // fallback value is a JSON object. parse it and add it as fallback content.
                context.PushElement({GetId(), GetInternalId(), true});
                std::shared_ptr<BaseElement> fallbackElement;
                T::ParseJsonObject(context, fallbackValue, fallbackElement);

                if (fallbackElement)
                {
                    m_fallbackType = FallbackType::Content;
                    m_fallbackContent = fallbackElement;
                    context.PopElement();
                    return;
                }
                throw AdaptiveCardParseException(ErrorStatusCode::InvalidPropertyValue, "Fallback content did not parse correctly.");
            }
            throw AdaptiveCardParseException(ErrorStatusCode::InvalidPropertyValue, "Invalid value for fallback");
        }
    }
}