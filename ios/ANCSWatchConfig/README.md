# ANCSWatchConfig

SwiftUI companion app scaffold cho dong ho `C3-ANCS`.

## Chuc nang

- Scan va ket noi toi dong ho qua CoreBluetooth
- Doc custom config service de xem catalog app ma dong ho da hoc duoc
- Chuyen trang catalog theo page
- Bat/tat tung app trong allowlist
- Bat/tat thong bao cuoc goi qua entry dac biet `__calls__`

## Giao thuc BLE

Service UUID:

- `605E4D3C-2B9A-019C-5B4A-3E4F00104A9F`

Characteristics:

- `Summary`: `605E4D3C-2B9A-019C-5B4A-3E4F01104A9F`
- `Page`: `605E4D3C-2B9A-019C-5B4A-3E4F02104A9F`
- `Catalog`: `605E4D3C-2B9A-019C-5B4A-3E4F03104A9F`
- `Toggle`: `605E4D3C-2B9A-019C-5B4A-3E4F04104A9F`

`Summary` la text UTF-8:

```text
version=1
page=0
pages=2
count=5
calls=1
revision=7
```

`Catalog` la text UTF-8 cho page hien tai:

```text
__calls__|1
com.zing.zalo|1
ph.telegra.Telegraph|0
```

`Toggle` ghi du lieu UTF-8:

```text
com.zing.zalo|1
```

Hoac:

```text
__calls__|0
```

## Cac buoc tren Mac

1. Cai `xcodegen`
2. Chay `xcodegen generate` trong thu muc `ios/ANCSWatchConfig`
3. Mo `ANCSWatchConfig.xcodeproj` trong Xcode
4. Chon iPhone that va chay

## Ghi chu

- Scaffold nay duoc tao tren Linux nen chua build duoc bang Xcode trong moi truong hien tai.
- App scan theo ten thiet bi `C3-ANCS`, khong yeu cau UUID custom phai xuat hien trong advertising packet.
