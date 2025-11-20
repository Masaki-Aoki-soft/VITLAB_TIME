import { NextRequest, NextResponse } from 'next/server';
import fs from 'fs';
import path from 'path';
import mime from 'mime-types';

export async function GET(
  request: NextRequest,
  { params }: { params: { path: string[] } }
) {
  try {
    const filePath = path.join(process.cwd(), ...params.path);
    
    // セキュリティチェック：プロジェクトルート外へのアクセスを防ぐ
    const resolvedPath = path.resolve(filePath);
    const projectRoot = path.resolve(process.cwd());
    if (!resolvedPath.startsWith(projectRoot)) {
      return NextResponse.json({ error: 'Forbidden' }, { status: 403 });
    }

    if (!fs.existsSync(filePath)) {
      return NextResponse.json({ error: 'Not Found' }, { status: 404 });
    }

    const stats = fs.statSync(filePath);
    if (stats.isDirectory()) {
      return NextResponse.json({ error: 'Not Found' }, { status: 404 });
    }

    const fileContent = fs.readFileSync(filePath);
    const mimeType = mime.lookup(filePath) || 'application/octet-stream';

    return new NextResponse(fileContent, {
      status: 200,
      headers: {
        'Content-Type': mimeType,
        'Cache-Control': 'public, max-age=3600',
      },
    });
  } catch (err: any) {
    return NextResponse.json(
      { error: err.message },
      { status: 500 }
    );
  }
}

