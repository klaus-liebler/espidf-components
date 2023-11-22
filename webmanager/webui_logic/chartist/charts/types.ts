import type {
  DataEvent,
  OptionsChangedEvent,
  DrawEvent,
  CreatedEvent
} from '../core/index';
import type { AnimationEvent } from '../svg/index';

export interface BaseChartEventsTypes<
  TCreateEvent = CreatedEvent,
  TDrawEvents = DrawEvent
> {
  data: DataEvent;
  options: OptionsChangedEvent;
  animationBegin: AnimationEvent;
  animationEnd: AnimationEvent;
  created: TCreateEvent;
  draw: TDrawEvents;
}
