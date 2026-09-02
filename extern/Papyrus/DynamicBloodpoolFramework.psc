Scriptname DynamicBloodpoolFramework Hidden

Int[] Function GetVersion() global native

Bool Function GetIniValueBool(String asValue, Bool abDefaultValue = false) global native

Float Function GetIniValueFloat(String asValue, Float afDefaultValue = 0.0) global native

Int Function GetIniValueInt(String asValue, Int aiDefaultValue = 0) global native

String Function GetIniValueString(String asValue, String asDefaultValue = "") global native

Bool Function GetDefaultIniValueBool(String asValue, Bool abFallback = false) global native

Float Function GetDefaultIniValueFloat(String asValue, Float afFallback = 0.0) global native

Int Function GetDefaultIniValueInt(String asValue, Int aiFallback = 0) global native

String Function GetDefaultIniValueString(String asValue, String asFallback = "") global native

Bool Function SetIniValueBool(String asKeySection, Bool abValue) global native

Bool Function SetIniValueFloat(String asKeySection, Float afValue) global native

Bool Function SetIniValueInt(String asKeySection, Int aiValue) global native

Bool Function SetIniValueString(String asKeySection, String asValue) global native

Function RequestRuntimeUpdate() Global Native

; ============================================================
; SpawnBloodpoolAtLocation
; ============================================================
; Spawns a bloodpool at a specific world position with optional rotation and overrides.
;
; Parameters:
;   profileID   - The bloodpool profile ID to use.
;   originRef   - The reference object from which the bloodpool originates (CAN be None).
;   posX, posY, posZ - World coordinates where the bloodpool will spawn.
;   rotationZ   - Optional rotation around the Z-axis (default 0.0).
;   scale       - Optional scale multiplier for the bloodpool (default -1.0, ignored if negative).
;   spread      - Optional spread override (default -1.0, ignored if negative).
;   durationMult- Optional duration multiplier (default -1.0, ignored if negative).
;
; Returns:
;   True if the bloodpool was successfully spawned, False otherwise.
Bool Function SpawnBloodpoolAtLocation(String profileID, ObjectReference originRef = None, \
    Float posX, Float posY, Float posZ, Float rotationZ = 0.0, \
    Float scale = -1.0, Float spread = -1.0, Float durationMult = -1.0) Global Native

; ============================================================
; SpawnBloodpoolAtNode
; ============================================================
; Spawns a bloodpool at a specific node of a reference with optional rotation and overrides.
;
; Parameters:
;   profileID   - The bloodpool profile ID to use.
;   originRef   - The reference object from which the bloodpool originates (CANNOT be None).
;   nodeName    - Name of the node on the originRef where the bloodpool will spawn.
;   rotationZ   - Optional rotation around the Z-axis (default 0.0).
;   scale       - Optional scale multiplier for the bloodpool (default -1.0, ignored if negative).
;   spread      - Optional spread override (default -1.0, ignored if negative).
;   durationMult- Optional duration multiplier (default -1.0, ignored if negative).
;
; Returns:
;   True if the bloodpool was successfully spawned, False otherwise.
Bool Function SpawnBloodpoolAtNode(String profileID, ObjectReference originRef = None, \
    String nodeName = "", Float rotationZ = 0.0, \
    Float scale = -1.0, Float spread = -1.0, Float durationMult = -1.0) Global Native