# Shopify catalog spec — VocalEssential

## A. Individual plugins  ($39.99 each, perpetual)

Recommended: ONE product "VocalEssential — Individual Plugins" with 15 variants,
OR 15 standalone products. Either way, the **variant SKU must equal the pluginId**
(lowercase) so the orders/paid webhook maps variant → plugin → KEYGEN_POLICY_SINGLES.

| Variant / Product      | SKU (REQUIRED)  | Price  | Type      | What it is                         |
|------------------------|-----------------|--------|-----------|------------------------------------|
| VocalGrit              | vocalgrit       | 39.99  | perpetual | Saturation / vocal texture         |
| VocalEss               | vocaless        | 39.99  | perpetual | De-esser                           |
| VocalQ                 | vocalq          | 39.99  | perpetual | Dynamic vocal EQ                   |
| VocalKnob              | vocalknob       | 39.99  | perpetual | One-knob vocal enhancer            |
| VocalAir               | vocalair        | 39.99  | perpetual | High-frequency "air" enhancer      |
| VocalComp              | vocalcomp       | 39.99  | perpetual | Compressor + gate                  |
| Vocal2A                | vocal2a         | 39.99  | perpetual | LA-2A-style optical leveler        |
| VocalTune              | vocaltune       | 39.99  | perpetual | Pitch correction / tuning          |
| VocalVerb              | vocalverb       | 39.99  | perpetual | Reverb                             |
| VocalDoubler           | vocaldoubler    | 39.99  | perpetual | Vocal doubler                      |
| VocalDelay             | vocaldelay      | 39.99  | perpetual | Tempo-synced delay                 |
| VocalGate              | vocalgate       | 39.99  | perpetual | Noise gate / expander              |
| VocalMod               | vocalmod        | 39.99  | perpetual | Modulation FX (chorus/flange/etc.) |
| VocalBlend             | vocalblend      | 39.99  | perpetual | Master-bus vocal/beat glue         |
| VocalChop              | vocalchop       | 39.99  | perpetual | Tempo-synced beat repeater/chopper |

## B. Suite — Lifetime  ($149.00, perpetual)
- Product: "VocalEssential Suite — Lifetime"
- SKU: `suite-lifetime`
- Price: 149.00, one-time
- Unlocks all 15 (minted under the SUITE Keygen product)

## C. Suite — Monthly  ($14.99/mo, subscription)
- Product: "VocalEssential Suite — Monthly"
- SKU: `suite-monthly`
- Price: 14.99 / month via Shopify-native Subscriptions selling plan
- Unlocks all 15 while active; locks on lapse/cancel (minted under SUITE product,
  monthly EXPIRING policy)

## Webhook → policy mapping (what orders/paid should do)
Routing is SKU-primary with a variant_id fallback (see orders-paid.ts), in order:
1. SKU in the 15 singles            → KEYGEN_POLICY_SINGLES[sku]  (per-plugin product)
2. has selling_plan OR `suite-monthly` → KEYGEN_POLICY_PRO         (SUITE product, expiry +35d)
3. SKU `suite-lifetime`             → KEYGEN_POLICY_SUITE          (SUITE product, no expiry)
4. else fall back to VARIANT_ENTITLEMENTS[variant_id] (existing products w/o SKUs)
5. else log loudly + skip (never mint an unknown entitlement)

NOTE: the monthly subscription is detected by the presence of a
selling_plan_allocation on the line item — NOT by price. If you recreate products,
set the SKUs as above; the variant_id fallback covers any product that doesn't yet
have its SKU set.
