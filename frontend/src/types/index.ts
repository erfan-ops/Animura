export interface ModuleInfo {
  name: string;
  version: string;
  previewPath: string;
  id: number;
}

export interface SchemaField {
  type?: 'int' | 'float' | 'bool' | 'select' | 'color' | 'color_list' | 'string';
  min?: number;
  max?: number;
  step?: number;
  options?: (string | number)[];
  [key: string]: unknown;
}

export type Schema = Record<string, SchemaField>;

export interface SettingsUI {
  schema: Schema;
  settings: Record<string, unknown>;
}

export type NotificationType = 'success' | 'warning' | 'error';

declare global {
  interface Window {
    chrome?: {
      webview?: {
        hostObjects: {
          nativeBridge: NativeBridge;
        };
        addEventListener(
          event: 'message',
          handler: (e: MessageEvent<string>) => void
        ): void;
        removeEventListener(
          event: 'message',
          handler: (e: MessageEvent<string>) => void
        ): void;
        postMessage(message: string): void;
      };
    };
  }
}

export interface NativeBridge {
  GetModulesList(): Promise<string>;
  GetRunningModuleId(): Promise<number>;
  StartWallpaper(moduleIndex: number): Promise<void>;
  StopWallpaper(): Promise<void>;
  LoadSettingsUI(moduleIndex: number): Promise<string>;
  ApplySettings(moduleIndex: number, settingsJson: string): Promise<void>;
  GetAccentColor(): Promise<string>;
}

export interface BackendMessage {
  type: 'runningModuleChanged';
  moduleId: number;
}
