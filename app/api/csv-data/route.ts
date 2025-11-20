import { NextResponse } from 'next/server';
import fs from 'fs';
import path from 'path';

export async function GET() {
  try {
    const csvPath = path.join(process.cwd(), 'oomiya_route_inf_4.csv');
    const csvData = fs.readFileSync(csvPath, 'utf8');
    return new NextResponse(csvData, {
      status: 200,
      headers: {
        'Content-Type': 'text/csv; charset=utf-8',
      },
    });
  } catch (err: any) {
    return NextResponse.json(
      { error: `CSV Error: ${err.message}` },
      { status: 500 }
    );
  }
}

