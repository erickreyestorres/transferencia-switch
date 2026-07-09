# Instalación mediante MTP

## Flujo 0.5.4

`5: SD Card install` es una vista funcional, no un directorio. Cuando Windows envía
un objeto, el servidor selecciona `IncomingObjectSink` por Storage ID y entrega el
flujo secuencial al adaptador `SdInstallReceiverFactory`.

1. Se aceptan `.nsp` y `.xci` sin compresión.
2. Para NSP, la cabecera PFS0 se mantiene en memoria y se validan cantidades, offsets, nombres,
   límites y tamaño total.
3. Para XCI se valida HFS0 raíz, se localiza `secure` y se valida su HFS0 interno.
4. Cada NCA se transmite directamente a un placeholder del `ContentStorage` SD. En
   XCI, su cabecera se convierte de distribución Gamecard a instalable mediante AES-XTS.
5. Al completar una NCA se sincroniza y registra mediante NCM.
6. El CNMT instalado se abre con `FsFileSystemType_ContentMeta`.
7. Se comprueba que todas las NCAs declaradas existan y coincidan en tamaño.
8. Si existen ticket y certificado emparejados, se importan mediante ES.
9. Se confirma `ContentMetaDatabase` y se publica `ApplicationRecord`.
10. Ante cancelación o error se eliminan el placeholder activo, los metadatos y las
   NCAs nuevas registradas por esa operación.
11. El servidor responde el resultado final MTP a Windows y solo después destruye el
   receptor y cierra sus sesiones NCM/NS/ES.
12. El ObjectHandle virtual permanece disponible para las consultas finales de Windows
   y se elimina al recibir `SessionEnded`; el paquete no se conserva en la SD.
13. Si el adaptador rechaza un bloque, el servidor drena los bytes restantes para no
   desalinear MTP y presenta el motivo original de la falla.
14. Una colección puede contener hasta 32 CNMT; cada registro se confirma y el rollback
   elimina todos los ContentMeta y contenidos nuevos de la operación.

El paquete completo no se guarda en FAT32, por lo que no se duplica el espacio. La
versión inicial sigue limitada a objetos MTP menores de 4 GiB porque el tamaño recibido
por el conjunto actual de operaciones es de 32 bits.

## Formatos posteriores

- NSZ/XCZ: descompresión NCZ por streaming con verificación del tamaño expandido.
- `6: NAND install`: el mismo caso de uso con `NcmStorageId_BuiltInUser`, habilitado
  solamente después de validar la ruta SD.
