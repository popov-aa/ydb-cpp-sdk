#pragma once

#include "codecs/codecs.h"
#include "common/counters.h"
#include "common/executor.h"
#include "common/retry_policy.h"

#include <src/api/grpc/ydb_topic_v1.grpc.pb.h>

#include <ydb-cpp-sdk/client/driver/driver.h>
#include <ydb-cpp-sdk/client/scheme/scheme.h>
#include <ydb-cpp-sdk/client/types/exceptions/exceptions.h>

#include <ydb-cpp-sdk/library/logger/log.h>

#include <ydb-cpp-sdk/util/string/builder.h>
#include <ydb-cpp-sdk/util/datetime/base.h>
#include <ydb-cpp-sdk/util/generic/size_literals.h>
#include <ydb-cpp-sdk/util/thread/pool.h>

#include <exception>
#include <variant>

namespace NYdb {
    class TProtoAccessor;

    namespace NScheme {
        struct TPermissions;
    }

    namespace NTable {
        class TTransaction;
    }
}

namespace NYdb::NTopic {

enum class EMeteringMode : ui32 {
    Unspecified = 0,
    ReservedCapacity = 1,
    RequestUnits = 2,

    Unknown = std::numeric_limits<int>::max(),
};


class TConsumer {
public:
    TConsumer(const Ydb::Topic::Consumer&);

    const std::string& GetConsumerName() const;
    bool GetImportant() const;
    const TInstant& GetReadFrom() const;
    const std::vector<ECodec>& GetSupportedCodecs() const;
    const std::map<std::string, std::string>& GetAttributes() const;

private:
    std::string ConsumerName_;
    bool Important_;
    TInstant ReadFrom_;
    std::map<std::string, std::string> Attributes_;
    std::vector<ECodec> SupportedCodecs_;

};


class TTopicStats {
public:
    TTopicStats(const Ydb::Topic::DescribeTopicResult::TopicStats& topicStats);

    ui64 GetStoreSizeBytes() const;
    TDuration GetMaxWriteTimeLag() const;
    TInstant GetMinLastWriteTime() const;
    ui64 GetBytesWrittenPerMinute() const;
    ui64 GetBytesWrittenPerHour() const;
    ui64 GetBytesWrittenPerDay() const;

private:
    ui64 StoreSizeBytes_;
    TInstant MinLastWriteTime_;
    TDuration MaxWriteTimeLag_;
    ui64 BytesWrittenPerMinute_;
    ui64 BytesWrittenPerHour_;
    ui64 BytesWrittenPerDay_;
};


class TPartitionStats {
public:
    TPartitionStats(const Ydb::Topic::PartitionStats& partitionStats);

    ui64 GetStartOffset() const;
    ui64 GetEndOffset() const;
    ui64 GetStoreSizeBytes() const;
    TDuration GetMaxWriteTimeLag() const;
    TInstant GetLastWriteTime() const;
    ui64 GetBytesWrittenPerMinute() const;
    ui64 GetBytesWrittenPerHour() const;
    ui64 GetBytesWrittenPerDay() const;

private:
    ui64 StartOffset_;
    ui64 EndOffset_;
    ui64 StoreSizeBytes_;
    TInstant LastWriteTime_;
    TDuration MaxWriteTimeLag_;
    ui64 BytesWrittenPerMinute_;
    ui64 BytesWrittenPerHour_;
    ui64 BytesWrittenPerDay_;

};

class TPartitionConsumerStats {
public:
    TPartitionConsumerStats(const Ydb::Topic::DescribeConsumerResult::PartitionConsumerStats& partitionStats);
    ui64 GetCommittedOffset() const;
    ui64 GetLastReadOffset() const;
    std::string GetReaderName() const;
    std::string GetReadSessionId() const;

private:
    ui64 CommittedOffset_;
    i64 LastReadOffset_;
    std::string ReaderName_;
    std::string ReadSessionId_;
};

// Topic partition location
class TPartitionLocation {
public:
    TPartitionLocation(const Ydb::Topic::PartitionLocation& partitionLocation);
    i32 GetNodeId() const;
    i64 GetGeneration() const;

private:
    // Node identificator.
    i32 NodeId_ = 1;

    // Partition generation.
    i64 Generation_ = 2;
};

class TPartitionInfo {
public:
    TPartitionInfo(const Ydb::Topic::DescribeTopicResult::PartitionInfo& partitionInfo);
    TPartitionInfo(const Ydb::Topic::DescribeConsumerResult::PartitionInfo& partitionInfo);

    ui64 GetPartitionId() const;
    bool GetActive() const;
    const std::vector<ui64> GetChildPartitionIds() const;
    const std::vector<ui64> GetParentPartitionIds() const;

    const std::optional<TPartitionStats>& GetPartitionStats() const;
    const std::optional<TPartitionConsumerStats>& GetPartitionConsumerStats() const;
    const std::optional<TPartitionLocation>& GetPartitionLocation() const;

private:
    ui64 PartitionId_;
    bool Active_;
    std::vector<ui64> ChildPartitionIds_;
    std::vector<ui64> ParentPartitionIds_;
    std::optional<TPartitionStats> PartitionStats_;
    std::optional<TPartitionConsumerStats> PartitionConsumerStats_;
    std::optional<TPartitionLocation> PartitionLocation_;
};

class TPartitioningSettings {
public:
    TPartitioningSettings() : MinActivePartitions_(0), PartitionCountLimit_(0){}
    TPartitioningSettings(const Ydb::Topic::PartitioningSettings& settings);
    TPartitioningSettings(ui64 minActivePartitions, ui64 partitionCountLimit)
        : MinActivePartitions_(minActivePartitions)
        , PartitionCountLimit_(partitionCountLimit) {
    }

    ui64 GetMinActivePartitions() const;
    ui64 GetPartitionCountLimit() const;
private:
    ui64 MinActivePartitions_;
    ui64 PartitionCountLimit_;
};

class TTopicDescription {
    friend class NYdb::TProtoAccessor;

public:
    TTopicDescription(Ydb::Topic::DescribeTopicResult&& desc);

    const std::string& GetOwner() const;

    const NScheme::TVirtualTimestamp& GetCreationTimestamp() const;

    const std::vector<NScheme::TPermissions>& GetPermissions() const;

    const std::vector<NScheme::TPermissions>& GetEffectivePermissions() const;

    const TPartitioningSettings& GetPartitioningSettings() const;

    ui32 GetTotalPartitionsCount() const;

    const std::vector<TPartitionInfo>& GetPartitions() const;

    const std::vector<ECodec>& GetSupportedCodecs() const;

    const TDuration& GetRetentionPeriod() const;

    std::optional<ui64> GetRetentionStorageMb() const;

    ui64 GetPartitionWriteSpeedBytesPerSecond() const;

    ui64 GetPartitionWriteBurstBytes() const;

    const std::map<std::string, std::string>& GetAttributes() const;

    const std::vector<TConsumer>& GetConsumers() const;

    EMeteringMode GetMeteringMode() const;

    const TTopicStats& GetTopicStats() const;

    void SerializeTo(Ydb::Topic::CreateTopicRequest& request) const;
private:

    const Ydb::Topic::DescribeTopicResult& GetProto() const;

    const Ydb::Topic::DescribeTopicResult Proto_;
    std::vector<TPartitionInfo> Partitions_;
    std::vector<ECodec> SupportedCodecs_;
    TPartitioningSettings PartitioningSettings_;
    TDuration RetentionPeriod_;
    std::optional<ui64> RetentionStorageMb_;
    ui64 PartitionWriteSpeedBytesPerSecond_;
    ui64 PartitionWriteBurstBytes_;
    EMeteringMode MeteringMode_;
    std::map<std::string, std::string> Attributes_;
    std::vector<TConsumer> Consumers_;

    TTopicStats TopicStats_;

    std::string Owner_;
    NScheme::TVirtualTimestamp CreationTimestamp_;
    std::vector<NScheme::TPermissions> Permissions_;
    std::vector<NScheme::TPermissions> EffectivePermissions_;
};


class TConsumerDescription {
    friend class NYdb::TProtoAccessor;

public:
    TConsumerDescription(Ydb::Topic::DescribeConsumerResult&& desc);

    const std::vector<TPartitionInfo>& GetPartitions() const;

    const TConsumer& GetConsumer() const;

private:

    const Ydb::Topic::DescribeConsumerResult& GetProto() const;


    const Ydb::Topic::DescribeConsumerResult Proto_;
    std::vector<TPartitionInfo> Partitions_;
    TConsumer Consumer_;
};

class TPartitionDescription {
    friend class NYdb::TProtoAccessor;

public:
    TPartitionDescription(Ydb::Topic::DescribePartitionResult&& desc);

    const TPartitionInfo& GetPartition() const;
private:
    const Ydb::Topic::DescribePartitionResult& GetProto() const;

    const Ydb::Topic::DescribePartitionResult Proto_;
    TPartitionInfo Partition_;
};

// Result for describe topic request.
struct TDescribeTopicResult : public TStatus {
    friend class NYdb::TProtoAccessor;


    TDescribeTopicResult(TStatus&& status, Ydb::Topic::DescribeTopicResult&& result);

    const TTopicDescription& GetTopicDescription() const;

private:
    TTopicDescription TopicDescription_;
};

// Result for describe consumer request.
struct TDescribeConsumerResult : public TStatus {
    friend class NYdb::TProtoAccessor;


    TDescribeConsumerResult(TStatus&& status, Ydb::Topic::DescribeConsumerResult&& result);

    const TConsumerDescription& GetConsumerDescription() const;

private:
    TConsumerDescription ConsumerDescription_;
};

// Result for describe partition request.
struct TDescribePartitionResult: public TStatus {
    friend class NYdb::TProtoAccessor;

    TDescribePartitionResult(TStatus&& status, Ydb::Topic::DescribePartitionResult&& result);

    const TPartitionDescription& GetPartitionDescription() const;

private:
    TPartitionDescription PartitionDescription_;
};

using TAsyncDescribeTopicResult = NThreading::TFuture<TDescribeTopicResult>;
using TAsyncDescribeConsumerResult = NThreading::TFuture<TDescribeConsumerResult>;
using TAsyncDescribePartitionResult = NThreading::TFuture<TDescribePartitionResult>;

template <class TSettings>
class TAlterAttributesBuilderImpl {
public:
    TAlterAttributesBuilderImpl(TSettings& parent)
    : Parent_(parent)
    { }

    TAlterAttributesBuilderImpl& Alter(const std::string& key, const std::string& value) {
        Parent_.AlterAttributes_[key] = value;
        return *this;
    }

    TAlterAttributesBuilderImpl& Add(const std::string& key, const std::string& value) {
        return Alter(key, value);
    }

    TAlterAttributesBuilderImpl& Drop(const std::string& key) {
        return Alter(key, "");
    }

    TSettings& EndAlterAttributes() { return Parent_; }

private:
    TSettings& Parent_;
};


struct TAlterConsumerSettings;
struct TAlterTopicSettings;

using TAlterConsumerAttributesBuilder = TAlterAttributesBuilderImpl<TAlterConsumerSettings>;
using TAlterTopicAttributesBuilder = TAlterAttributesBuilderImpl<TAlterTopicSettings>;

template<class TSettings>
struct TConsumerSettings {
    using TSelf = TConsumerSettings;

    using TAttributes = std::map<std::string, std::string>;

    TConsumerSettings(TSettings& parent): Parent_(parent) {}
    TConsumerSettings(TSettings& parent, const std::string& name) : ConsumerName_(name), Parent_(parent) {}

    FLUENT_SETTING(std::string, ConsumerName);
    FLUENT_SETTING_DEFAULT(bool, Important, false);
    FLUENT_SETTING_DEFAULT(TInstant, ReadFrom, TInstant::Zero());

    FLUENT_SETTING_VECTOR(ECodec, SupportedCodecs);

    FLUENT_SETTING(TAttributes, Attributes);

    TConsumerSettings& AddAttribute(const std::string& key, const std::string& value) {
        Attributes_[key] = value;
        return *this;
    }

    TConsumerSettings& SetAttributes(std::map<std::string, std::string>&& attributes) {
        Attributes_ = std::move(attributes);
        return *this;
    }

    TConsumerSettings& SetAttributes(const std::map<std::string, std::string>& attributes) {
        Attributes_ = attributes;
        return *this;
    }

    TConsumerSettings& SetSupportedCodecs(std::vector<ECodec>&& codecs) {
        SupportedCodecs_ = std::move(codecs);
        return *this;
    }

    TConsumerSettings& SetSupportedCodecs(const std::vector<ECodec>& codecs) {
        SupportedCodecs_ = codecs;
        return *this;
    }

    TSettings& EndAddConsumer() { return Parent_; };

private:
    TSettings& Parent_;
};


struct TAlterConsumerSettings {
    using TSelf = TAlterConsumerSettings;

    using TAlterAttributes = std::map<std::string, std::string>;

    TAlterConsumerSettings(TAlterTopicSettings& parent): Parent_(parent) {}
    TAlterConsumerSettings(TAlterTopicSettings& parent, const std::string& name) : ConsumerName_(name), Parent_(parent) {}

    FLUENT_SETTING(std::string, ConsumerName);
    FLUENT_SETTING_OPTIONAL(bool, SetImportant);
    FLUENT_SETTING_OPTIONAL(TInstant, SetReadFrom);

    FLUENT_SETTING_OPTIONAL_VECTOR(ECodec, SetSupportedCodecs);

    FLUENT_SETTING(TAlterAttributes, AlterAttributes);

    TAlterConsumerAttributesBuilder BeginAlterAttributes() {
        return TAlterConsumerAttributesBuilder(*this);
    }

    TAlterConsumerSettings& SetSupportedCodecs(std::vector<ECodec>&& codecs) {
        SetSupportedCodecs_ = std::move(codecs);
        return *this;
    }

    TAlterConsumerSettings& SetSupportedCodecs(const std::vector<ECodec>& codecs) {
        SetSupportedCodecs_ = codecs;
        return *this;
    }

    TAlterTopicSettings& EndAlterConsumer() { return Parent_; };

private:
    TAlterTopicSettings& Parent_;
};


struct TCreateTopicSettings : public TOperationRequestSettings<TCreateTopicSettings> {

    using TSelf = TCreateTopicSettings;
    using TAttributes = std::map<std::string, std::string>;

    FLUENT_SETTING(TPartitioningSettings, PartitioningSettings);

    FLUENT_SETTING_DEFAULT(TDuration, RetentionPeriod, TDuration::Hours(24));

    FLUENT_SETTING_VECTOR(ECodec, SupportedCodecs);

    FLUENT_SETTING_DEFAULT(ui64, RetentionStorageMb, 0);
    FLUENT_SETTING_DEFAULT(EMeteringMode, MeteringMode, EMeteringMode::Unspecified);

    FLUENT_SETTING_DEFAULT(ui64, PartitionWriteSpeedBytesPerSecond, 0);
    FLUENT_SETTING_DEFAULT(ui64, PartitionWriteBurstBytes, 0);

    FLUENT_SETTING_VECTOR(TConsumerSettings<TCreateTopicSettings>, Consumers);

    FLUENT_SETTING(TAttributes, Attributes);


    TCreateTopicSettings& SetSupportedCodecs(std::vector<ECodec>&& codecs) {
        SupportedCodecs_ = std::move(codecs);
        return *this;
    }

    TCreateTopicSettings& SetSupportedCodecs(const std::vector<ECodec>& codecs) {
        SupportedCodecs_ = codecs;
        return *this;
    }

    TConsumerSettings<TCreateTopicSettings>& BeginAddConsumer() {
        Consumers_.push_back({*this});
        return Consumers_.back();
    }

    TConsumerSettings<TCreateTopicSettings>& BeginAddConsumer(const std::string& name) {
        Consumers_.push_back({*this, name});
        return Consumers_.back();
    }

    TCreateTopicSettings& AddAttribute(const std::string& key, const std::string& value) {
        Attributes_[key] = value;
        return *this;
    }

    TCreateTopicSettings& SetAttributes(std::map<std::string, std::string>&& attributes) {
        Attributes_ = std::move(attributes);
        return *this;
    }

    TCreateTopicSettings& SetAttributes(const std::map<std::string, std::string>& attributes) {
        Attributes_ = attributes;
        return *this;
    }

    TCreateTopicSettings& PartitioningSettings(ui64 minActivePartitions, ui64 partitionCountLimit) {
        PartitioningSettings_ = TPartitioningSettings(minActivePartitions, partitionCountLimit);
        return *this;
    }
};


struct TAlterTopicSettings : public TOperationRequestSettings<TAlterTopicSettings> {

    using TSelf = TAlterTopicSettings;
    using TAlterAttributes = std::map<std::string, std::string>;

    FLUENT_SETTING_OPTIONAL(TPartitioningSettings, AlterPartitioningSettings);

    FLUENT_SETTING_OPTIONAL(TDuration, SetRetentionPeriod);

    FLUENT_SETTING_OPTIONAL_VECTOR(ECodec, SetSupportedCodecs);

    FLUENT_SETTING_OPTIONAL(ui64, SetRetentionStorageMb);

    FLUENT_SETTING_OPTIONAL(ui64, SetPartitionWriteSpeedBytesPerSecond);
    FLUENT_SETTING_OPTIONAL(ui64, SetPartitionWriteBurstBytes);

    FLUENT_SETTING_OPTIONAL(EMeteringMode, SetMeteringMode);

    FLUENT_SETTING_VECTOR(TConsumerSettings<TAlterTopicSettings>, AddConsumers);
    FLUENT_SETTING_VECTOR(std::string, DropConsumers);
    FLUENT_SETTING_VECTOR(TAlterConsumerSettings, AlterConsumers);

    FLUENT_SETTING(TAlterAttributes, AlterAttributes);

    TAlterTopicAttributesBuilder BeginAlterAttributes() {
        return TAlterTopicAttributesBuilder(*this);
    }

    TAlterTopicSettings& SetSupportedCodecs(std::vector<ECodec>&& codecs) {
        SetSupportedCodecs_ = std::move(codecs);
        return *this;
    }

    TAlterTopicSettings& SetSupportedCodecs(const std::vector<ECodec>& codecs) {
        SetSupportedCodecs_ = codecs;
        return *this;
    }

    TConsumerSettings<TAlterTopicSettings>& BeginAddConsumer() {
        AddConsumers_.push_back({*this});
        return AddConsumers_.back();
    }

    TConsumerSettings<TAlterTopicSettings>& BeginAddConsumer(const std::string& name) {
        AddConsumers_.push_back({*this, name});
        return AddConsumers_.back();
    }

    TAlterConsumerSettings& BeginAlterConsumer() {
        AlterConsumers_.push_back({*this});
        return AlterConsumers_.back();
    }

    TAlterConsumerSettings& BeginAlterConsumer(const std::string& name) {
        AlterConsumers_.push_back({*this, name});
        return AlterConsumers_.back();
    }

    TAlterTopicSettings& AlterPartitioningSettings(ui64 minActivePartitions, ui64 partitionCountLimit) {
        AlterPartitioningSettings_ = TPartitioningSettings(minActivePartitions, partitionCountLimit);
        return *this;
    }
};


// Settings for drop resource request.
struct TDropTopicSettings : public TOperationRequestSettings<TDropTopicSettings> {
    using TOperationRequestSettings<TDropTopicSettings>::TOperationRequestSettings;
};

// Settings for describe topic request.
struct TDescribeTopicSettings : public TOperationRequestSettings<TDescribeTopicSettings> {
    using TSelf = TDescribeTopicSettings;

    FLUENT_SETTING_DEFAULT(bool, IncludeStats, false);

    FLUENT_SETTING_DEFAULT(bool, IncludeLocation, false);
};

// Settings for describe consumer request.
struct TDescribeConsumerSettings : public TOperationRequestSettings<TDescribeConsumerSettings> {
    using TSelf = TDescribeConsumerSettings;

    FLUENT_SETTING_DEFAULT(bool, IncludeStats, false);

    FLUENT_SETTING_DEFAULT(bool, IncludeLocation, false);
};

// Settings for describe partition request.
struct TDescribePartitionSettings: public TOperationRequestSettings<TDescribePartitionSettings> {
    using TSelf = TDescribePartitionSettings;

    FLUENT_SETTING_DEFAULT(bool, IncludeStats, false);

    FLUENT_SETTING_DEFAULT(bool, IncludeLocation, false);
};

// Settings for commit offset request.
struct TCommitOffsetSettings : public TOperationRequestSettings<TCommitOffsetSettings> {};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename TEvent>
class TPrintable {
public:
    std::string DebugString(bool printData = false) const {
        TStringBuilder b;
        static_cast<const TEvent*>(this)->DebugString(b, printData);
        return b;
    }

    // implemented in template specializations
    void DebugString(TStringBuilder& ret, bool printData = false) const = delete;
};

//! Session metainformation.
struct TWriteSessionMeta: public TThrRefBase {
    using TPtr = TIntrusivePtr<TWriteSessionMeta>;

    //! User defined fields.
    std::unordered_map<std::string, std::string> Fields;
};

struct TMessageMeta: public TThrRefBase {
    using TPtr = TIntrusivePtr<TMessageMeta>;

    //! User defined fields.
    std::vector<std::pair<std::string, std::string>> Fields;
};

//! Event that is sent to client during session destruction.
struct TSessionClosedEvent: public TStatus, public TPrintable<TSessionClosedEvent> {
    using TStatus::TStatus;
};

template<>
void TPrintable<TSessionClosedEvent>::DebugString(TStringBuilder& res, bool) const;

struct TWriteStat : public TThrRefBase {
    TDuration WriteTime;
    TDuration MinTimeInPartitionQueue;
    TDuration MaxTimeInPartitionQueue;
    TDuration PartitionQuotedTime;
    TDuration TopicQuotedTime;
    using TPtr = TIntrusivePtr<TWriteStat>;
};

class TContinuationToken : public TNonCopyable {
    friend class TContinuationTokenIssuer;

    bool Valid = true;

public:
    TContinuationToken& operator=(TContinuationToken&& other) {
        if (!other.Valid) {
            ythrow TContractViolation("Cannot move invalid token");
        }
        Valid = std::exchange(other.Valid, false);
        return *this;
    }

    TContinuationToken(TContinuationToken&& other) {
        *this = std::move(other);
    }

private:
    TContinuationToken() = default;
};

class TContinuationTokenIssuer {
protected:
    static auto IssueContinuationToken() {
        return TContinuationToken{};
    }
};

//! Partition session.
struct TPartitionSession: public TThrRefBase, public TPrintable<TPartitionSession> {
    using TPtr = TIntrusivePtr<TPartitionSession>;

public:
    //! Request partition session status.
    //! Result will come to TPartitionSessionStatusEvent.
    virtual void RequestStatus() = 0;

    //!
    //! Properties.
    //!

    //! Unique identifier of partition session.
    //! It is unique within one read session.
    ui64 GetPartitionSessionId() const {
        return PartitionSessionId;
    }

    //! Topic path.
    const std::string& GetTopicPath() const {
        return TopicPath;
    }

    //! Partition id.
    ui64 GetPartitionId() const {
        return PartitionId;
    }

protected:
    ui64 PartitionSessionId;
    std::string TopicPath;
    ui64 PartitionId;
};

template<>
void TPrintable<TPartitionSession>::DebugString(TStringBuilder& res, bool) const;

//! Events for read session.
struct TReadSessionEvent {
    class TPartitionSessionAccessor {
    public:
        TPartitionSessionAccessor(TPartitionSession::TPtr partitionSession);

        virtual ~TPartitionSessionAccessor() = default;

        virtual const TPartitionSession::TPtr& GetPartitionSession() const;

    protected:
        TPartitionSession::TPtr PartitionSession;
    };

    //! Event with new data.
    //! Contains batch of messages from single partition session.
    struct TDataReceivedEvent : public TPartitionSessionAccessor, public TPrintable<TDataReceivedEvent> {
        struct TMessageInformation {
            TMessageInformation(ui64 offset,
                                std::string producerId,
                                ui64 seqNo,
                                TInstant createTime,
                                TInstant writeTime,
                                TWriteSessionMeta::TPtr meta,
                                TMessageMeta::TPtr messageMeta,
                                ui64 uncompressedSize,
                                std::string messageGroupId);
            ui64 Offset;
            std::string ProducerId;
            ui64 SeqNo;
            TInstant CreateTime;
            TInstant WriteTime;
            TWriteSessionMeta::TPtr Meta;
            TMessageMeta::TPtr MessageMeta;
            ui64 UncompressedSize;
            std::string MessageGroupId;
        };

        class TMessageBase : public TPrintable<TMessageBase> {
        public:
            TMessageBase(const std::string& data, TMessageInformation info);

            virtual ~TMessageBase() = default;

            virtual const std::string& GetData() const;

            virtual void Commit() = 0;

            //! Message offset.
            ui64 GetOffset() const;

            //! Producer id.
            const std::string& GetProducerId() const;

            //! Message group id.
            const std::string& GetMessageGroupId() const;

            //! Sequence number.
            ui64 GetSeqNo() const;

            //! Message creation timestamp.
            TInstant GetCreateTime() const;

            //! Message write timestamp.
            TInstant GetWriteTime() const;

            //! Metainfo.
            const TWriteSessionMeta::TPtr& GetMeta() const;

            //! Message level meta info.
            const TMessageMeta::TPtr& GetMessageMeta() const;

        protected:
            std::string Data;
            TMessageInformation Information;
        };

        //! Single message.
        struct TMessage: public TMessageBase, public TPartitionSessionAccessor, public TPrintable<TMessage> {
            using TPrintable<TMessage>::DebugString;

            TMessage(const std::string& data, std::exception_ptr decompressionException, TMessageInformation information,
                     TPartitionSession::TPtr partitionSession);

            //! User data.
            //! Throws decompressor exception if decompression failed.
            const std::string& GetData() const override;

            //! Commits single message.
            void Commit() override;

            bool HasException() const;

        private:
            std::exception_ptr DecompressionException;
        };

        struct TCompressedMessage: public TMessageBase,
                                   public TPartitionSessionAccessor,
                                   public TPrintable<TCompressedMessage> {
            using TPrintable<TCompressedMessage>::DebugString;

            TCompressedMessage(ECodec codec, const std::string& data, TMessageInformation information,
                               TPartitionSession::TPtr partitionSession);

            virtual ~TCompressedMessage() {
            }

            //! Message codec
            ECodec GetCodec() const;

            //! Uncompressed size.
            ui64 GetUncompressedSize() const;

            //! Commits all offsets in compressed message.
            void Commit() override;

        private:
            ECodec Codec;
        };

    public:
        TDataReceivedEvent(std::vector<TMessage> messages, std::vector<TCompressedMessage> compressedMessages,
                           TPartitionSession::TPtr partitionSession);

        bool HasCompressedMessages() const {
            return !CompressedMessages.empty();
        }

        size_t GetMessagesCount() const {
            return Messages.size() + CompressedMessages.size();
        }

        //! Get messages.
        std::vector<TMessage>& GetMessages() {
            CheckMessagesFilled(false);
            return Messages;
        }

        const std::vector<TMessage>& GetMessages() const {
            CheckMessagesFilled(false);
            return Messages;
        }

        //! Get compressed messages.
        std::vector<TCompressedMessage>& GetCompressedMessages() {
            CheckMessagesFilled(true);
            return CompressedMessages;
        }

        const std::vector<TCompressedMessage>& GetCompressedMessages() const {
            CheckMessagesFilled(true);
            return CompressedMessages;
        }

        //! Commits all messages in batch.
        void Commit();

    private:
        void CheckMessagesFilled(bool compressed) const {
            Y_ABORT_UNLESS(!Messages.empty() || !CompressedMessages.empty());
            if (compressed && CompressedMessages.empty()) {
                ythrow yexception() << "cannot get compressed messages, parameter decompress=true for read session";
            }
            if (!compressed && Messages.empty()) {
                ythrow yexception() << "cannot get decompressed messages, parameter decompress=false for read session";
            }
        }

    private:
        std::vector<TMessage> Messages;
        std::vector<TCompressedMessage> CompressedMessages;
        std::vector<std::pair<ui64, ui64>> OffsetRanges;
    };

    //! Acknowledgement for commit request.
    struct TCommitOffsetAcknowledgementEvent: public TPartitionSessionAccessor,
                                              public TPrintable<TCommitOffsetAcknowledgementEvent> {
        TCommitOffsetAcknowledgementEvent(TPartitionSession::TPtr partitionSession, ui64 committedOffset);

        //! Committed offset.
        //! This means that from now the first available
        //! message offset in current partition
        //! for current consumer is this offset.
        //! All messages before are committed and futher never be available.
        ui64 GetCommittedOffset() const {
            return CommittedOffset;
        }

    private:
        ui64 CommittedOffset;
    };

    //! Server command for creating and starting partition session.
    struct TStartPartitionSessionEvent: public TPartitionSessionAccessor,
                                        public TPrintable<TStartPartitionSessionEvent> {
        explicit TStartPartitionSessionEvent(TPartitionSession::TPtr, ui64 committedOffset, ui64 endOffset);

        //! Current committed offset in partition session.
        ui64 GetCommittedOffset() const {
            return CommittedOffset;
        }

        //! Offset of first not existing message in partition session.
        ui64 GetEndOffset() const {
            return EndOffset;
        }

        //! Confirm partition session creation.
        //! This signals that user is ready to receive data from this partition session.
        //! If maybe is empty then no rewinding
        void Confirm(std::optional<ui64> readOffset = std::nullopt, std::optional<ui64> commitOffset = std::nullopt);

    private:
        ui64 CommittedOffset;
        ui64 EndOffset;
    };

    //! Server command for stopping and destroying partition session.
    //! Server can destroy partition session gracefully
    //! for rebalancing among all topic clients.
    struct TStopPartitionSessionEvent: public TPartitionSessionAccessor, public TPrintable<TStopPartitionSessionEvent> {
        TStopPartitionSessionEvent(TPartitionSession::TPtr partitionSession, bool committedOffset);

        //! Last offset of the partition session that was committed.
        ui64 GetCommittedOffset() const {
            return CommittedOffset;
        }

        //! Confirm partition session destruction.
        //! Confirm has no effect if TPartitionSessionClosedEvent for same partition session with is received.
        void Confirm();

    private:
        ui64 CommittedOffset;
    };

    //! Status for partition session requested via TPartitionSession::RequestStatus()
    struct TPartitionSessionStatusEvent: public TPartitionSessionAccessor,
                                         public TPrintable<TPartitionSessionStatusEvent> {
        TPartitionSessionStatusEvent(TPartitionSession::TPtr partitionSession, ui64 committedOffset, ui64 readOffset,
                                     ui64 endOffset, TInstant writeTimeHighWatermark);

        //! Committed offset.
        ui64 GetCommittedOffset() const {
            return CommittedOffset;
        }

        //! Offset of next message (that is not yet read by session).
        ui64 GetReadOffset() const {
            return ReadOffset;
        }

        //! Offset of first not existing message in partition.
        ui64 GetEndOffset() const {
            return EndOffset;
        }

        //! Write time high watermark.
        //! Write timestamp of next message written to this partition will be no less than this.
        TInstant GetWriteTimeHighWatermark() const {
            return WriteTimeHighWatermark;
        }

    private:
        ui64 CommittedOffset = 0;
        ui64 ReadOffset = 0;
        ui64 EndOffset = 0;
        TInstant WriteTimeHighWatermark;
    };

    //! Event that signals user about
    //! partition session death.
    //! This could be after graceful stop of partition session
    //! or when connection with partition was lost.
    struct TPartitionSessionClosedEvent: public TPartitionSessionAccessor,
                                         public TPrintable<TPartitionSessionClosedEvent> {
        enum class EReason {
            StopConfirmedByUser,
            Lost,
            ConnectionLost,
        };

    public:
        TPartitionSessionClosedEvent(TPartitionSession::TPtr partitionSession, EReason reason);

        EReason GetReason() const {
            return Reason;
        }

    private:
        EReason Reason;
    };

    using TEvent = std::variant<TDataReceivedEvent,
                                TCommitOffsetAcknowledgementEvent,
                                TStartPartitionSessionEvent,
                                TStopPartitionSessionEvent,
                                TPartitionSessionStatusEvent,
                                TPartitionSessionClosedEvent,
                                TSessionClosedEvent>;
};

//! Set of offsets to commit.
//! Class that could store offsets in order to commit them later.
//! This class is not thread safe.
class TDeferredCommit {
public:
    //! Add message to set.
    void Add(const TReadSessionEvent::TDataReceivedEvent::TMessage& message);

    //! Add all messages from dataReceivedEvent to set.
    void Add(const TReadSessionEvent::TDataReceivedEvent& dataReceivedEvent);

    //! Add offsets range to set.
    void Add(const TPartitionSession::TPtr& partitionSession, ui64 startOffset, ui64 endOffset);

    //! Add offset to set.
    void Add(const TPartitionSession::TPtr& partitionSession, ui64 offset);

    //! Commit all added offsets.
    void Commit();

    TDeferredCommit();
    TDeferredCommit(const TDeferredCommit&) = delete;
    TDeferredCommit(TDeferredCommit&&);
    TDeferredCommit& operator=(const TDeferredCommit&) = delete;
    TDeferredCommit& operator=(TDeferredCommit&&);

    ~TDeferredCommit();

private:
    class TImpl;
    THolder<TImpl> Impl;
};

//! Events debug strings.
template<>
void TPrintable<TReadSessionEvent::TDataReceivedEvent::TMessageBase>::DebugString(TStringBuilder& ret, bool printData) const;
template<>
void TPrintable<TReadSessionEvent::TDataReceivedEvent::TMessage>::DebugString(TStringBuilder& ret, bool printData) const;
template<>
void TPrintable<TReadSessionEvent::TDataReceivedEvent::TCompressedMessage>::DebugString(TStringBuilder& ret, bool printData) const;
template<>
void TPrintable<TReadSessionEvent::TDataReceivedEvent>::DebugString(TStringBuilder& ret, bool printData) const;
template<>
void TPrintable<TReadSessionEvent::TCommitOffsetAcknowledgementEvent>::DebugString(TStringBuilder& ret, bool printData) const;
template<>
void TPrintable<TReadSessionEvent::TStartPartitionSessionEvent>::DebugString(TStringBuilder& ret, bool printData) const;
template<>
void TPrintable<TReadSessionEvent::TStopPartitionSessionEvent>::DebugString(TStringBuilder& ret, bool printData) const;
template<>
void TPrintable<TReadSessionEvent::TPartitionSessionStatusEvent>::DebugString(TStringBuilder& ret, bool printData) const;
template<>
void TPrintable<TReadSessionEvent::TPartitionSessionClosedEvent>::DebugString(TStringBuilder& ret, bool printData) const;
template<>
void TPrintable<TSessionClosedEvent>::DebugString(TStringBuilder& ret, bool printData) const;

std::string DebugString(const TReadSessionEvent::TEvent& event);

//! Events for write session.
struct TWriteSessionEvent {

    //! Event with acknowledge for written messages.
    struct TWriteAck {
        //! Write result.
        enum EEventState {
            EES_WRITTEN, //! Successfully written.
            EES_ALREADY_WRITTEN, //! Skipped on SeqNo deduplication.
            EES_DISCARDED //! In case of destruction of writer or retry policy discarded future retries in this writer.
        };
        //! Details of successfully written message.
        struct TWrittenMessageDetails {
            ui64 Offset;
            ui64 PartitionId;
        };
        //! Same SeqNo as provided on write.
        ui64 SeqNo;
        EEventState State;
        //! Filled only for EES_WRITTEN. Empty for ALREADY and DISCARDED.
        std::optional<TWrittenMessageDetails> Details;
        //! Write stats from server. See TWriteStat. nullptr for DISCARDED event.
        TWriteStat::TPtr Stat;
    };

    struct TAcksEvent : public TPrintable<TAcksEvent> {
        //! Acks could be batched from several Write requests.
        //! They are provided to client as soon as possible.
        std::vector<TWriteAck> Acks;
    };

    //! Indicates that a writer is ready to accept new message(s).
    //! Continuation token should be kept and then used in write methods.
    struct TReadyToAcceptEvent : public TPrintable<TReadyToAcceptEvent> {
        mutable TContinuationToken ContinuationToken;

        TReadyToAcceptEvent() = delete;
        TReadyToAcceptEvent(TContinuationToken&& t) : ContinuationToken(std::move(t)) {
        }
        TReadyToAcceptEvent(TReadyToAcceptEvent&&) = default;
        TReadyToAcceptEvent(const TReadyToAcceptEvent& other) : ContinuationToken(std::move(other.ContinuationToken)) {
        }
        TReadyToAcceptEvent& operator=(TReadyToAcceptEvent&&) = default;
        TReadyToAcceptEvent& operator=(const TReadyToAcceptEvent& other) {
            ContinuationToken = std::move(other.ContinuationToken);
            return *this;
        }
    };

    using TEvent = std::variant<TAcksEvent, TReadyToAcceptEvent, TSessionClosedEvent>;
};

//! Events debug strings.
template<>
void TPrintable<TWriteSessionEvent::TAcksEvent>::DebugString(TStringBuilder& ret, bool printData) const;
template<>
void TPrintable<TWriteSessionEvent::TReadyToAcceptEvent>::DebugString(TStringBuilder& ret, bool printData) const;

std::string DebugString(const TWriteSessionEvent::TEvent& event);

using TSessionClosedHandler = std::function<void(const TSessionClosedEvent&)>;

//! Settings for write session.
struct TWriteSessionSettings : public TRequestSettings<TWriteSessionSettings> {
    using TSelf = TWriteSessionSettings;

    TWriteSessionSettings() = default;
    TWriteSessionSettings(const TWriteSessionSettings&) = default;
    TWriteSessionSettings(TWriteSessionSettings&&) = default;
    TWriteSessionSettings(const std::string& path, const std::string& producerId, const std::string& messageGroupId) {
        Path(path);
        ProducerId(producerId);
        MessageGroupId(messageGroupId);
    }

    TWriteSessionSettings& operator=(const TWriteSessionSettings&) = default;
    TWriteSessionSettings& operator=(TWriteSessionSettings&&) = default;

    //! Path of topic to write.
    FLUENT_SETTING(std::string, Path);

    //! ProducerId (aka SourceId) to use.
    FLUENT_SETTING(std::string, ProducerId);

    //! MessageGroupId to use.
    FLUENT_SETTING(std::string, MessageGroupId);

    //! Explicitly enables or disables deduplication for this write session.
    //! If ProducerId option is defined deduplication will always be enabled.
    //! If ProducerId option is empty, but deduplication is enable, a random ProducerId is generated.
    FLUENT_SETTING_OPTIONAL(bool, DeduplicationEnabled);

    //! Write to an exact partition. Generally server assigns partition automatically by message_group_id.
    //! Using this option is not recommended unless you know for sure why you need it.
    FLUENT_SETTING_OPTIONAL(ui32, PartitionId);

    //! Direct write to the partition host.
    //! If both PartitionId and DirectWriteToPartition are set, write session goes directly to the partition host.
    //! If DirectWriteToPartition set without PartitionId, the write session is established in three stages:
    //! 1. Get a partition ID.
    //! 2. Find out the location of the partition by its ID.
    //! 3. Connect directly to the partition host.
    FLUENT_SETTING_DEFAULT(bool, DirectWriteToPartition, true);

    //! codec and level to use for data compression prior to write.
    FLUENT_SETTING_DEFAULT(ECodec, Codec, ECodec::GZIP);
    FLUENT_SETTING_DEFAULT(i32, CompressionLevel, 4);

    //! Writer will not accept new messages if memory usage exceeds this limit.
    //! Memory usage consists of raw data pending compression and compressed messages being sent.
    FLUENT_SETTING_DEFAULT(ui64, MaxMemoryUsage, 20_MB);

    //! Maximum messages accepted by writer but not written (with confirmation from server).
    //! Writer will not accept new messages after reaching the limit.
    FLUENT_SETTING_DEFAULT(ui32, MaxInflightCount, 100000);

    //! Retry policy enables automatic retries for non-fatal errors.
    //! IRetryPolicy::GetDefaultPolicy() if null (not set).
    FLUENT_SETTING(IRetryPolicy::TPtr, RetryPolicy);

    //! User metadata that may be attached to write session.
    TWriteSessionSettings& AppendSessionMeta(const std::string& key, const std::string& value) {
        Meta_.Fields[key] = value;
        return *this;
    };

    TWriteSessionMeta Meta_;

    //! Writer will accumulate messages until reaching up to BatchFlushSize bytes
    //! but for no longer than BatchFlushInterval.
    //! Upon reaching FlushInterval or FlushSize limit, all messages will be written with one batch.
    //! Greatly increases performance for small messages.
    //! Setting either value to zero means immediate write with no batching. (Unrecommended, especially for clients
    //! sending small messages at high rate).
    FLUENT_SETTING_OPTIONAL(TDuration, BatchFlushInterval);
    FLUENT_SETTING_OPTIONAL(ui64, BatchFlushSizeBytes);

    FLUENT_SETTING_DEFAULT(TDuration, ConnectTimeout, TDuration::Seconds(30));

    FLUENT_SETTING_OPTIONAL(TWriterCounters::TPtr, Counters);

    //! Executor for compression tasks.
    //! If not set, default executor will be used.
    FLUENT_SETTING(IExecutor::TPtr, CompressionExecutor);

    struct TEventHandlers {
        using TSelf = TEventHandlers;
        using TWriteAckHandler = std::function<void(TWriteSessionEvent::TAcksEvent&)>;
        using TReadyToAcceptHandler = std::function<void(TWriteSessionEvent::TReadyToAcceptEvent&)>;

        //! Function to handle Acks events.
        //! If this handler is set, write ack events will be handled by handler,
        //! otherwise sent to TWriteSession::GetEvent().
        FLUENT_SETTING(TWriteAckHandler, AcksHandler);

        //! Function to handle ReadyToAccept event.
        //! If this handler is set, write these events will be handled by handler,
        //! otherwise sent to TWriteSession::GetEvent().
        FLUENT_SETTING(TReadyToAcceptHandler, ReadyToAcceptHandler);

        //! Function to handle close session events.
        //! If this handler is set, close session events will be handled by handler
        //! and then sent to TWriteSession::GetEvent().
        FLUENT_SETTING(TSessionClosedHandler, SessionClosedHandler);

        //! Function to handle all event types.
        //! If event with current type has no handler for this type of event,
        //! this handler (if specified) will be used.
        //! If this handler is not specified, event can be received with TWriteSession::GetEvent() method.
        std::function<void(TWriteSessionEvent::TEvent&)> CommonHandler_;
        TSelf& CommonHandler(std::function<void(TWriteSessionEvent::TEvent&)>&& handler) {
            CommonHandler_ = std::move(handler);
            return static_cast<TSelf&>(*this);
        }

        //! Executor for handlers.
        //! If not set, default single threaded executor will be used.
        FLUENT_SETTING(IExecutor::TPtr, HandlersExecutor);

        [[deprecated("Typo in name. Use ReadyToAcceptHandler instead.")]]
        TSelf& ReadyToAcceptHander(const TReadyToAcceptHandler& value) {
            return ReadyToAcceptHandler(value);
        }
    };

    //! Event handlers.
    FLUENT_SETTING(TEventHandlers, EventHandlers);

    //! Enables validation of SeqNo. If enabled, then writer will check writing with seqNo and without it and throws exception.
    FLUENT_SETTING_DEFAULT(bool, ValidateSeqNo, true);
};

//! Read settings for single topic.
struct TTopicReadSettings {
    using TSelf = TTopicReadSettings;

    TTopicReadSettings() = default;
    TTopicReadSettings(const TTopicReadSettings&) = default;
    TTopicReadSettings(TTopicReadSettings&&) = default;
    TTopicReadSettings(const std::string& path) {
        Path(path);
    }

    TTopicReadSettings& operator=(const TTopicReadSettings&) = default;
    TTopicReadSettings& operator=(TTopicReadSettings&&) = default;

    //! Path of topic to read.
    FLUENT_SETTING(std::string, Path);

    //! Start reading from this timestamp.
    FLUENT_SETTING_OPTIONAL(TInstant, ReadFromTimestamp);

    //! Partition ids to read.
    //! 0-based.
    FLUENT_SETTING_VECTOR(ui64, PartitionIds);

    //! Max message time lag. All messages older that now - MaxLag will be ignored.
    FLUENT_SETTING_OPTIONAL(TDuration, MaxLag);
};

//! Settings for read session.
struct TReadSessionSettings: public TRequestSettings<TReadSessionSettings> {
    using TSelf = TReadSessionSettings;

    struct TEventHandlers {
        using TSelf = TEventHandlers;

        //! Set simple handler with data processing and also
        //! set other handlers with default behaviour.
        //! They automatically commit data after processing
        //! and confirm partition session events.
        //!
        //! Sets the following handlers:
        //! DataReceivedHandler: sets DataReceivedHandler to handler that calls dataHandler and (if
        //! commitDataAfterProcessing is set) then calls Commit(). CommitAcknowledgementHandler to handler that does
        //! nothing. StartPartitionSessionHandler to handler that confirms event. StopPartitionSessionHandler to
        //! handler that confirms event. PartitionSessionStatusHandler to handler that does nothing.
        //! PartitionSessionClosedHandler to handler that does nothing.
        //!
        //! dataHandler: handler of data event.
        //! commitDataAfterProcessing: automatically commit data after calling of dataHandler.
        //! gracefulReleaseAfterCommit: wait for commit acknowledgements for all inflight data before confirming
        //! partition session stop.
        TSelf& SimpleDataHandlers(std::function<void(TReadSessionEvent::TDataReceivedEvent&)> dataHandler,
                                  bool commitDataAfterProcessing = false, bool gracefulStopAfterCommit = true);

        //! Data size limit for the DataReceivedHandler handler.
        //! The data size may exceed this limit.
        FLUENT_SETTING_DEFAULT(size_t, MaxMessagesBytes, Max<size_t>());

        //! Function to handle data events.
        //! If this handler is set, data events will be handled by handler,
        //! otherwise sent to TReadSession::GetEvent().
        //! Default value is empty function (not set).
        FLUENT_SETTING(std::function<void(TReadSessionEvent::TDataReceivedEvent&)>, DataReceivedHandler);

        //! Function to handle commit ack events.
        //! If this handler is set, commit ack events will be handled by handler,
        //! otherwise sent to TReadSession::GetEvent().
        //! Default value is empty function (not set).
        FLUENT_SETTING(std::function<void(TReadSessionEvent::TCommitOffsetAcknowledgementEvent&)>,
                       CommitOffsetAcknowledgementHandler);

        //! Function to handle start partition session events.
        //! If this handler is set, create partition session events will be handled by handler,
        //! otherwise sent to TReadSession::GetEvent().
        //! Default value is empty function (not set).
        FLUENT_SETTING(std::function<void(TReadSessionEvent::TStartPartitionSessionEvent&)>,
                       StartPartitionSessionHandler);

        //! Function to handle stop partition session events.
        //! If this handler is set, destroy partition session events will be handled by handler,
        //! otherwise sent to TReadSession::GetEvent().
        //! Default value is empty function (not set).
        FLUENT_SETTING(std::function<void(TReadSessionEvent::TStopPartitionSessionEvent&)>,
                       StopPartitionSessionHandler);

        //! Function to handle partition session status events.
        //! If this handler is set, partition session status events will be handled by handler,
        //! otherwise sent to TReadSession::GetEvent().
        //! Default value is empty function (not set).
        FLUENT_SETTING(std::function<void(TReadSessionEvent::TPartitionSessionStatusEvent&)>,
                       PartitionSessionStatusHandler);

        //! Function to handle partition session closed events.
        //! If this handler is set, partition session closed events will be handled by handler,
        //! otherwise sent to TReadSession::GetEvent().
        //! Default value is empty function (not set).
        FLUENT_SETTING(std::function<void(TReadSessionEvent::TPartitionSessionClosedEvent&)>,
                       PartitionSessionClosedHandler);

        //! Function to handle session closed events.
        //! If this handler is set, close session events will be handled by handler
        //! and then sent to TReadSession::GetEvent().
        //! Default value is empty function (not set).
        FLUENT_SETTING(TSessionClosedHandler, SessionClosedHandler);

        //! Function to handle all event types.
        //! If event with current type has no handler for this type of event,
        //! this handler (if specified) will be used.
        //! If this handler is not specified, event can be received with TReadSession::GetEvent() method.
        FLUENT_SETTING(std::function<void(TReadSessionEvent::TEvent&)>, CommonHandler);

        //! Executor for handlers.
        //! If not set, default single threaded executor will be used.
        FLUENT_SETTING(IExecutor::TPtr, HandlersExecutor);
    };


    std::string ConsumerName_ = "";
    //! Consumer.
    TSelf& ConsumerName(const std::string& name) {
        ConsumerName_ = name;
        WithoutConsumer_ = false;
        return static_cast<TSelf&>(*this);
    }

    bool WithoutConsumer_ = false;
    //! Read without consumer.
    TSelf& WithoutConsumer() {
        WithoutConsumer_ = true;
        ConsumerName_ = "";
        return static_cast<TSelf&>(*this);
    }

    //! Topics.
    FLUENT_SETTING_VECTOR(TTopicReadSettings, Topics);

    //! Maximum memory usage for read session.
    FLUENT_SETTING_DEFAULT(size_t, MaxMemoryUsageBytes, 100_MB);

    //! Max message time lag. All messages older that now - MaxLag will be ignored.
    FLUENT_SETTING_OPTIONAL(TDuration, MaxLag);

    //! Start reading from this timestamp.
    FLUENT_SETTING_OPTIONAL(TInstant, ReadFromTimestamp);

    //! Policy for reconnections.
    //! IRetryPolicy::GetDefaultPolicy() if null (not set).
    FLUENT_SETTING(IRetryPolicy::TPtr, RetryPolicy);

    //! Event handlers.
    //! See description in TEventHandlers class.
    FLUENT_SETTING(TEventHandlers, EventHandlers);

    //! Decompress messages
    FLUENT_SETTING_DEFAULT(bool, Decompress, true);

    //! Executor for decompression tasks.
    //! If not set, default executor will be used.
    FLUENT_SETTING(IExecutor::TPtr, DecompressionExecutor);

    //! Counters.
    //! If counters are not provided explicitly,
    //! they will be created inside session (without link with parent counters).
    FLUENT_SETTING(TReaderCounters::TPtr, Counters);

    FLUENT_SETTING_DEFAULT(TDuration, ConnectTimeout, TDuration::Seconds(30));

    //! Log.
    FLUENT_SETTING_OPTIONAL(TLog, Log);
};

//! Contains the message to write and all the options.
struct TWriteMessage {
    using TSelf = TWriteMessage;
    using TMessageMeta = std::vector<std::pair<std::string, std::string>>;
public:
    TWriteMessage() = delete;
    TWriteMessage(std::string_view data)
        : Data(data)
    {}

    //! A message that is already compressed by codec. Codec from WriteSessionSettings does not apply to this message.
    //! Compression will not be performed in SDK for such messages.
    static TWriteMessage CompressedMessage(const std::string_view& data, ECodec codec, ui32 originalSize) {
        TWriteMessage result{data};
        result.Codec = codec;
        result.OriginalSize = originalSize;
        return result;
    }

    bool Compressed() const {
        return Codec.has_value();
    }

    //! Message body.
    std::string_view Data;

    //! Codec and original size for compressed message.
    //! Do not specify or change these options directly, use CompressedMessage()
    //! method to create an object for compressed message.
    std::optional<ECodec> Codec;
    ui32 OriginalSize = 0;

    //! Message SeqNo, optional. If not provided SDK core will calculate SeqNo automatically.
    //! NOTICE: Either all messages within one write session must have SeqNo provided or none of them.
    FLUENT_SETTING_OPTIONAL(ui64, SeqNo);

    //! Message creation timestamp. If not provided, Now() will be used.
    FLUENT_SETTING_OPTIONAL(TInstant, CreateTimestamp);

    //! Message metadata. Limited to 4096 characters overall (all keys and values combined).
    FLUENT_SETTING(TMessageMeta, MessageMeta);

    //! Transaction id
    FLUENT_SETTING_OPTIONAL(std::reference_wrapper<NTable::TTransaction>, Tx);

    const NTable::TTransaction* GetTxPtr() const
    {
        return Tx_ ? &Tx_->get() : nullptr;
    }
};

//! Simple write session. Does not need event handlers. Does not provide Events, ContinuationTokens, write Acks.
class ISimpleBlockingWriteSession : public TThrRefBase {
public:
    //! Write single message. Blocks for up to blockTimeout if inflight is full or memoryUsage is exceeded;
    //! return - true if write succeeded, false if message was not enqueued for write within blockTimeout.
    //! no Ack is provided.
    virtual bool Write(TWriteMessage&& message, const TDuration& blockTimeout = TDuration::Max()) = 0;


    //! Write single message. Deprecated method with only basic message options.
    virtual bool Write(std::string_view data, std::optional<ui64> seqNo = std::nullopt, std::optional<TInstant> createTimestamp = std::nullopt,
                       const TDuration& blockTimeout = TDuration::Max()) = 0;

    //! Blocks till SeqNo is discovered from server. Returns 0 in case of failure on init.
    virtual ui64 GetInitSeqNo() = 0;

    //! Complete all active writes, wait for ack from server and close.
    //! closeTimeout - max time to wait. Empty Maybe means infinity.
    //! return - true if all writes were completed and acked. false if timeout was reached and some writes were aborted.

    virtual bool Close(TDuration closeTimeout = TDuration::Max()) = 0;

    //! Returns true if write session is alive and acitve. False if session was closed.
    virtual bool IsAlive() const = 0;

    virtual TWriterCounters::TPtr GetCounters() = 0;

    //! Close immediately and destroy, don't wait for anything.
    virtual ~ISimpleBlockingWriteSession() = default;
};

//! Generic write session with all capabilities.
class IWriteSession {
public:
    //! Future that is set when next event is available.
    virtual NThreading::TFuture<void> WaitEvent() = 0;

    //! Wait and return next event. Use WaitEvent() for non-blocking wait.
    virtual std::optional<TWriteSessionEvent::TEvent> GetEvent(bool block = false) = 0;

    //! Get several events in one call.
    //! If blocking = false, instantly returns up to maxEventsCount available events.
    //! If blocking = true, blocks till maxEventsCount events are available.
    //! If maxEventsCount is unset, write session decides the count to return itself.
    virtual std::vector<TWriteSessionEvent::TEvent> GetEvents(bool block = false, std::optional<size_t> maxEventsCount = std::nullopt) = 0;

    //! Future that is set when initial SeqNo is available.
    virtual NThreading::TFuture<ui64> GetInitSeqNo() = 0;

    //! Write single message.
    //! continuationToken - a token earlier provided to client with ReadyToAccept event.
    virtual void Write(TContinuationToken&& continuationToken, TWriteMessage&& message) = 0;

    //! Write single message. Old method with only basic message options.
    virtual void Write(TContinuationToken&& continuationToken, std::string_view data, std::optional<ui64> seqNo = std::nullopt,
                       std::optional<TInstant> createTimestamp = std::nullopt) = 0;

    //! Write single message that is already coded by codec.
    //! continuationToken - a token earlier provided to client with ReadyToAccept event.
    virtual void WriteEncoded(TContinuationToken&& continuationToken, TWriteMessage&& params) = 0;

    //! Write single message that is already compressed by codec. Old method with only basic message options.
    virtual void WriteEncoded(TContinuationToken&& continuationToken, std::string_view data, ECodec codec, ui32 originalSize,
                              std::optional<ui64> seqNo = std::nullopt, std::optional<TInstant> createTimestamp = std::nullopt) = 0;


    //! Wait for all writes to complete (no more that closeTimeout()), then close.
    //! Return true if all writes were completed and acked, false if timeout was reached and some writes were aborted.
    virtual bool Close(TDuration closeTimeout = TDuration::Max()) = 0;

    //! Writer counters with different stats (see TWriterConuters).
    virtual TWriterCounters::TPtr GetCounters() = 0;

    //! Close() with timeout = 0 and destroy everything instantly.
    virtual ~IWriteSession() = default;
};

struct TReadSessionGetEventSettings : public TCommonClientSettingsBase<TReadSessionGetEventSettings> {
    using TSelf = TReadSessionGetEventSettings;

    FLUENT_SETTING_DEFAULT(bool, Block, false);
    FLUENT_SETTING_OPTIONAL(size_t, MaxEventsCount);
    FLUENT_SETTING_DEFAULT(size_t, MaxByteSize, std::numeric_limits<size_t>::max());
    FLUENT_SETTING_OPTIONAL(std::reference_wrapper<NTable::TTransaction>, Tx);
};

class IReadSession {
public:
    //! Main reader loop.
    //! Wait for next reader event.
    virtual NThreading::TFuture<void> WaitEvent() = 0;

    //! Main reader loop.
    //! Get read session events.
    //! Blocks until event occurs if "block" is set.
    //!
    //! maxEventsCount: maximum events count in batch.
    //! maxByteSize: total size limit of data messages in batch.
    //! block: block until event occurs.
    //!
    //! If maxEventsCount is not specified,
    //! read session chooses event batch size automatically.
    virtual std::vector<TReadSessionEvent::TEvent> GetEvents(bool block = false, std::optional<size_t> maxEventsCount = std::nullopt,
                                                         size_t maxByteSize = std::numeric_limits<size_t>::max()) = 0;

    virtual std::vector<TReadSessionEvent::TEvent> GetEvents(const TReadSessionGetEventSettings& settings) = 0;

    //! Get single event.
    virtual std::optional<TReadSessionEvent::TEvent> GetEvent(bool block = false,
                                                       size_t maxByteSize = std::numeric_limits<size_t>::max()) = 0;

    virtual std::optional<TReadSessionEvent::TEvent> GetEvent(const TReadSessionGetEventSettings& settings) = 0;

    //! Close read session.
    //! Waits for all commit acknowledgments to arrive.
    //! Force close after timeout.
    //! This method is blocking.
    //! When session is closed,
    //! TSessionClosedEvent arrives.
    virtual bool Close(TDuration timeout = TDuration::Max()) = 0;

    //! Reader counters with different stats (see TReaderConuters).
    virtual TReaderCounters::TPtr GetCounters() const = 0;

    //! Get unique identifier of read session.
    virtual std::string GetSessionId() const = 0;

    virtual ~IReadSession() = default;
};

struct TTopicClientSettings : public TCommonClientSettingsBase<TTopicClientSettings> {
    using TSelf = TTopicClientSettings;

    //! Default executor for compression tasks.
    FLUENT_SETTING_DEFAULT(IExecutor::TPtr, DefaultCompressionExecutor, CreateThreadPoolExecutor(2));

    //! Default executor for callbacks.
    FLUENT_SETTING_DEFAULT(IExecutor::TPtr, DefaultHandlersExecutor, CreateThreadPoolExecutor(1));
};

// Topic client.
class TTopicClient {
public:
    class TImpl;

    TTopicClient(const TDriver& driver, const TTopicClientSettings& settings = TTopicClientSettings());

    void ProvideCodec(ECodec codecId, THolder<ICodec>&& codecImpl);

    // Create a new topic.
    TAsyncStatus CreateTopic(const std::string& path, const TCreateTopicSettings& settings = {});

    // Update a topic.
    TAsyncStatus AlterTopic(const std::string& path, const TAlterTopicSettings& settings = {});

    // Delete a topic.
    TAsyncStatus DropTopic(const std::string& path, const TDropTopicSettings& settings = {});

    // Describe a topic.
    TAsyncDescribeTopicResult DescribeTopic(const std::string& path, const TDescribeTopicSettings& settings = {});

    // Describe a topic consumer.
    TAsyncDescribeConsumerResult DescribeConsumer(const std::string& path, const std::string& consumer, const TDescribeConsumerSettings& settings = {});

    // Describe a topic partition
    TAsyncDescribePartitionResult DescribePartition(const std::string& path, i64 partitionId, const TDescribePartitionSettings& settings = {});

    //! Create read session.
    std::shared_ptr<IReadSession> CreateReadSession(const TReadSessionSettings& settings);

    //! Create write session.
    std::shared_ptr<ISimpleBlockingWriteSession> CreateSimpleBlockingWriteSession(const TWriteSessionSettings& settings);
    std::shared_ptr<IWriteSession> CreateWriteSession(const TWriteSessionSettings& settings);

    // Commit offset
    TAsyncStatus CommitOffset(const std::string& path, ui64 partitionId, const std::string& consumerName, ui64 offset,
        const TCommitOffsetSettings& settings = {});

protected:
    void OverrideCodec(ECodec codecId, THolder<ICodec>&& codecImpl);

private:
    std::shared_ptr<TImpl> Impl_;
};

} // namespace NYdb::NTopic
