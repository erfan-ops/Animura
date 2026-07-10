/**
 * @file types/index.ts
 * @brief TypeScript type definitions shared across the React frontend.
 *
 * Defines the shapes of data exchanged with the C++ backend through the
 * NativeBridge COM object, the WebView2 global type augmentation, and
 * UI-level types like notification variants.
 */

/** Metadata for a single wallpaper module displayed in the grid. */
export interface ModuleInfo {
  /** Display name from module.json (e.g., "Star Simulator"). */
  name: string;

  /** Description from module.json (e.g., "Renders dots that connect to the mouse cursor with distance-based lines"). */
  description: string;

  /** Semantic version string from module.json (e.g., "1.0.0"). */
  version: string;
  /**
   * Virtual-host URL for the preview image.
   *
   * Format: `https://animura.modules/<folder>/<file>`.
   * Resolved by WebView2 to the local `modules/` directory via
   * the virtual host mapping set up in WebView2Host.
   */
  previewPath: string;
  /** Zero-based index into the module catalog. Used as the stable ID for all backend calls. */
  id: number;
}

/**
 * A single field definition in a module's JSON schema.
 *
 * The `type` field determines which React control is rendered by
 * SettingsControl. If `type` is absent, the field is treated as a
 * nested group (recursive SettingsControl).
 */
export interface SchemaField {
  /** Setting type — drives control selection in SettingsControl. */
  type?: 'int' | 'float' | 'bool' | 'select' | 'color' | 'color_list' | 'string' | 'file';
  /** Minimum value (for int/float sliders). */
  min?: number;
  /** Maximum value (for int/float sliders). */
  max?: number;
  /** Step increment (for int/float sliders). */
  step?: number;
  /** Allowed values (for select dropdown). */
  options?: (string | number)[];
  /** Allows arbitrary additional properties for nested schema objects. */
  [key: string]: unknown;
}

/** Top-level schema: a mapping of setting keys to their field definitions. */
export type Schema = Record<string, SchemaField>;

/** Response from NativeBridge.LoadSettingsUI — the schema and current values. */
export interface SettingsUI {
  schema: Schema;
  settings: Record<string, unknown>;
}

/** Toast notification severity level. */
export type NotificationType = 'success' | 'warning' | 'error';

/**
 * Augments the global `Window` interface with the WebView2 host objects API.
 *
 * When running outside WebView2 (e.g., in a browser for development),
 * `window.chrome` is undefined and the bridge functions return empty/error values.
 */
declare global {
  interface Window {
    chrome?: {
      webview?: {
        hostObjects: {
          /** The NativeBridge COM IDispatch object registered by WebView2Host. */
          nativeBridge: NativeBridge;
        };
        /** Subscribe to C++ → JS web messages (e.g., runningModuleChanged). */
        addEventListener(
          event: 'message',
          handler: (e: MessageEvent<string>) => void
        ): void;
        /** Unsubscribe from C++ → JS web messages. */
        removeEventListener(
          event: 'message',
          handler: (e: MessageEvent<string>) => void
        ): void;
        /** Send a message to C++ via the web message channel. */
        postMessage(message: string): void;
      };
    };
  }
}

/**
 * The raw COM bridge interface as seen from JavaScript.
 *
 * **Important:** Despite the TypeScript types, all methods return BSTR
 * (string) at runtime. `GetRunningModuleId()` returns a string like `"0"`
 * and must be parsed with `Number()`. The typed wrapper in
 * `frontend/src/bridge/native.ts` handles this coercion.
 */
export interface NativeBridge {
  /** Returns a JSON string — array of ModuleInfo objects. */
  GetModulesList(): Promise<string>;
  /** Returns the running module ID as a STRING (e.g., "0" or "-1"). Parse with Number(). */
  GetRunningModuleId(): Promise<number>;
  /** Starts the wallpaper at the given module index. */
  StartWallpaper(moduleIndex: number): Promise<void>;
  /** Stops the currently running wallpaper. */
  StopWallpaper(): Promise<void>;
  /** Returns a JSON string — `{schema, settings}` for the settings panel. */
  LoadSettingsUI(moduleIndex: number): Promise<string>;
  /** Writes settings JSON to disk (atomic via QSaveFile). */
  ApplySettings(moduleIndex: number, settingsJson: string): Promise<void>;
  /** Opens a native file dialog and returns the selected file's full path. */
  PickFile(filter?: string): Promise<string>;
  /**
   * Opens a native file dialog for *.zip files, validates the package,
   * extracts it to modules/<id>/, and refreshes the module catalog.
   * Returns "OK" on success, or "ERROR:<message>" on failure.
   */
  InstallModule(): Promise<string>;
}

/** Shape of a C++ → JS web message. */
export interface BackendMessage {
  type: 'runningModuleChanged' | 'modulesChanged';
  moduleId?: number;
}
