// automatically generated by the FlatBuffers compiler, do not modify

import * as flatbuffers from 'flatbuffers';

import { Message, unionToMessage, unionListToMessage } from '../webmanager/message';


export class MessageWrapper {
  bb: flatbuffers.ByteBuffer|null = null;
  bb_pos = 0;
  __init(i:number, bb:flatbuffers.ByteBuffer):MessageWrapper {
  this.bb_pos = i;
  this.bb = bb;
  return this;
}

static getRootAsMessageWrapper(bb:flatbuffers.ByteBuffer, obj?:MessageWrapper):MessageWrapper {
  return (obj || new MessageWrapper()).__init(bb.readInt32(bb.position()) + bb.position(), bb);
}

static getSizePrefixedRootAsMessageWrapper(bb:flatbuffers.ByteBuffer, obj?:MessageWrapper):MessageWrapper {
  bb.setPosition(bb.position() + flatbuffers.SIZE_PREFIX_LENGTH);
  return (obj || new MessageWrapper()).__init(bb.readInt32(bb.position()) + bb.position(), bb);
}

messageType():Message {
  const offset = this.bb!.__offset(this.bb_pos, 4);
  return offset ? this.bb!.readUint8(this.bb_pos + offset) : Message.NONE;
}

message<T extends flatbuffers.Table>(obj:any):any|null {
  const offset = this.bb!.__offset(this.bb_pos, 6);
  return offset ? this.bb!.__union(obj, this.bb_pos + offset) : null;
}

static startMessageWrapper(builder:flatbuffers.Builder) {
  builder.startObject(2);
}

static addMessageType(builder:flatbuffers.Builder, messageType:Message) {
  builder.addFieldInt8(0, messageType, Message.NONE);
}

static addMessage(builder:flatbuffers.Builder, messageOffset:flatbuffers.Offset) {
  builder.addFieldOffset(1, messageOffset, 0);
}

static endMessageWrapper(builder:flatbuffers.Builder):flatbuffers.Offset {
  const offset = builder.endObject();
  return offset;
}

static finishMessageWrapperBuffer(builder:flatbuffers.Builder, offset:flatbuffers.Offset) {
  builder.finish(offset);
}

static finishSizePrefixedMessageWrapperBuffer(builder:flatbuffers.Builder, offset:flatbuffers.Offset) {
  builder.finish(offset, undefined, true);
}

static createMessageWrapper(builder:flatbuffers.Builder, messageType:Message, messageOffset:flatbuffers.Offset):flatbuffers.Offset {
  MessageWrapper.startMessageWrapper(builder);
  MessageWrapper.addMessageType(builder, messageType);
  MessageWrapper.addMessage(builder, messageOffset);
  return MessageWrapper.endMessageWrapper(builder);
}
}
